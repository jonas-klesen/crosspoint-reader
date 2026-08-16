#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace studycore {

inline constexpr std::string_view MATH_ASSET_NAMESPACE = "studypet-math-v1";
inline constexpr std::string_view MATH_ASSET_DIRECTORY = "/study/assets/math/v1/";

/// Return the formula between exact standalone $$ delimiters.
inline bool extractMathFormula(const std::string_view line, std::string_view& formula) noexcept {
  if (line.size() < 5 || !line.starts_with("$$") || !line.ends_with("$$")) {
    formula = {};
    return false;
  }
  formula = line.substr(2, line.size() - 4);
  if (formula.find('\n') != std::string_view::npos || formula.find('\r') != std::string_view::npos) {
    formula = {};
    return false;
  }
  return !formula.empty();
}

inline constexpr uint64_t FNV1A_OFFSET_BASIS = 14695981039346656037ULL;
inline constexpr uint64_t FNV1A_PRIME = 1099511628211ULL;

inline uint64_t mathAssetKey(const std::string_view formula) noexcept {
  uint64_t hash = FNV1A_OFFSET_BASIS;
  const auto addByte = [&hash](const uint8_t byte) {
    hash ^= byte;
    hash *= FNV1A_PRIME;
  };
  for (const char value : MATH_ASSET_NAMESPACE) addByte(static_cast<uint8_t>(value));
  addByte(0);
  for (const char value : formula) addByte(static_cast<uint8_t>(value));
  return hash;
}

/// Build a bounded absolute path from a formula key.
inline bool formatMathAssetPath(const uint64_t key, char* output, const std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0) return false;
  const int written = std::snprintf(output, capacity, "%.*s%016llx.spm", static_cast<int>(MATH_ASSET_DIRECTORY.size()),
                                    MATH_ASSET_DIRECTORY.data(), static_cast<unsigned long long>(key));
  return written >= 0 && static_cast<std::size_t>(written) < capacity;
}

}  // namespace studycore
