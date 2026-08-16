#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace studypet {

// Directory names are read through the 256-byte HalFile::getName() buffer.
// The repository rejects the exact-fit boundary because it cannot distinguish
// a complete name from a truncated one.
inline constexpr std::size_t MAX_DECK_COMPONENT_BYTES = 254;

// Bounds one owning location string while allowing eight typical maximum-size
// components plus separators. This is an application safety bound, not a CSV
// format limit; entries beyond it are skipped during directory listing.
inline constexpr std::size_t MAX_DECK_LOCATION_BYTES = 2048;

inline bool isSafeDeckFilename(const std::string_view filename) {
  if (filename.empty() || filename == "." || filename == "..") return false;
  if (filename.size() > MAX_DECK_COMPONENT_BYTES) return false;
  if (filename.find('/') != std::string_view::npos || filename.find('\\') != std::string_view::npos) return false;
  return std::all_of(filename.begin(), filename.end(),
                     [](const char value) { return static_cast<unsigned char>(value) >= 0x20 && value != 0x7f; });
}

inline bool hasCsvExtension(const std::string_view filename) {
  constexpr std::string_view extension = ".csv";
  if (filename.size() < extension.size()) return false;
  const std::size_t offset = filename.size() - extension.size();
  return std::equal(extension.begin(), extension.end(), filename.begin() + static_cast<std::ptrdiff_t>(offset),
                    [](const char left, const char right) {
                      return std::tolower(static_cast<unsigned char>(left)) ==
                             std::tolower(static_cast<unsigned char>(right));
                    });
}

inline bool isDeckCandidate(const std::string_view filename) {
  if (!isSafeDeckFilename(filename) || filename.front() == '.') return false;
  if (filename.ends_with("~") || filename.ends_with(".tmp") || filename.ends_with(".part")) return false;
  return hasCsvExtension(filename);
}

inline std::string deckDisplayName(const std::string_view filename) {
  constexpr std::string_view extension = ".csv";
  if (!hasCsvExtension(filename)) return std::string(filename);
  return std::string(filename.substr(0, filename.size() - extension.size()));
}

inline bool isSafeRelativePathComponent(const std::string_view component) {
  return isSafeDeckFilename(component) && component.front() != '.';
}

inline bool isSafeRelativePath(const std::string_view path, const bool allowEmpty) {
  if (path.empty()) return allowEmpty;
  if (path.size() > MAX_DECK_LOCATION_BYTES || path.front() == '/' || path.back() == '/') return false;
  if (path.find('\\') != std::string_view::npos) return false;

  std::size_t componentStart = 0;
  for (std::size_t i = 0; i <= path.size(); ++i) {
    if (i != path.size() && path[i] != '/') continue;
    if (!isSafeRelativePathComponent(path.substr(componentStart, i - componentStart))) return false;
    componentStart = i + 1;
  }
  return true;
}

inline bool isSafeRelativeDirectory(const std::string_view path) { return isSafeRelativePath(path, true); }

inline bool isSafeRelativeDeckPath(const std::string_view path) {
  return isSafeRelativePath(path, false) && hasCsvExtension(path);
}

inline bool joinRelativeDeckPath(const std::string_view parent, const std::string_view child, std::string& output) {
  if (!isSafeRelativeDirectory(parent) || !isSafeRelativePathComponent(child)) return false;
  const std::size_t length = parent.empty() ? child.size() : parent.size() + 1 + child.size();
  if (length > MAX_DECK_LOCATION_BYTES) return false;

  output.clear();
  output.reserve(length);
  if (!parent.empty()) {
    output.append(parent);
    output.push_back('/');
  }
  output.append(child);
  return true;
}

inline std::string relativePathBasename(const std::string_view path) {
  const std::size_t separator = path.find_last_of('/');
  return std::string(path.substr(separator == std::string_view::npos ? 0 : separator + 1));
}

inline std::string deckDisplayNameForRelativePath(const std::string_view path) {
  return deckDisplayName(relativePathBasename(path));
}

// Same numeric-aware, case-insensitive ordering used by CrossPoint's file
// browser, kept here so StudyPet's directory grouping is host-testable.
inline bool naturalNameLess(const std::string_view left, const std::string_view right) {
  const auto isDigit = [](const char value) { return std::isdigit(static_cast<unsigned char>(value)) != 0; };
  std::size_t leftIndex = 0;
  std::size_t rightIndex = 0;

  while (leftIndex < left.size() && rightIndex < right.size()) {
    if (isDigit(left[leftIndex]) && isDigit(right[rightIndex])) {
      std::size_t leftSignificant = leftIndex;
      std::size_t rightSignificant = rightIndex;
      while (leftSignificant < left.size() && left[leftSignificant] == '0') ++leftSignificant;
      while (rightSignificant < right.size() && right[rightSignificant] == '0') ++rightSignificant;

      std::size_t leftEnd = leftSignificant;
      std::size_t rightEnd = rightSignificant;
      while (leftEnd < left.size() && isDigit(left[leftEnd])) ++leftEnd;
      while (rightEnd < right.size() && isDigit(right[rightEnd])) ++rightEnd;

      const std::size_t leftDigits = leftEnd - leftSignificant;
      const std::size_t rightDigits = rightEnd - rightSignificant;
      if (leftDigits != rightDigits) return leftDigits < rightDigits;
      for (std::size_t offset = 0; offset < leftDigits; ++offset) {
        if (left[leftSignificant + offset] != right[rightSignificant + offset]) {
          return left[leftSignificant + offset] < right[rightSignificant + offset];
        }
      }
      leftIndex = leftEnd;
      rightIndex = rightEnd;
      continue;
    }

    const int leftChar = std::tolower(static_cast<unsigned char>(left[leftIndex]));
    const int rightChar = std::tolower(static_cast<unsigned char>(right[rightIndex]));
    if (leftChar != rightChar) return leftChar < rightChar;
    ++leftIndex;
    ++rightIndex;
  }

  return leftIndex == left.size() && rightIndex != right.size();
}

inline bool deckEntryLess(const bool leftIsFolder, const std::string_view leftName, const bool rightIsFolder,
                          const std::string_view rightName) {
  if (leftIsFolder != rightIsFolder) return leftIsFolder;
  if (naturalNameLess(leftName, rightName)) return true;
  if (naturalNameLess(rightName, leftName)) return false;
  return leftName < rightName;
}

}  // namespace studypet
