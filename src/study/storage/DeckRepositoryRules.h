#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace studypet {

inline bool isSafeDeckFilename(const std::string_view filename) {
  if (filename.empty() || filename == "." || filename == "..") return false;
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

}  // namespace studypet
