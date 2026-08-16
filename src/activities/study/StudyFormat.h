#pragma once

// Small, allocation-free formatting and narrowing helpers for StudyPet UI
// code. Header-only so the firmware build shares one implementation and the
// host unit tests can compile it directly. No exceptions, no allocation, no
// Arduino dependencies.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

namespace studypet {

// Format into a fixed buffer with std::snprintf. Returns true only when the
// whole output fit (no truncation) and snprintf reported no error. On failure
// the buffer is replaced with `fallback` (when non-null) and false is
// returned, so UI callers never display silently truncated text.
template <typename... Args>
bool safeFormat(char* buffer, const std::size_t bufferSize, const char* fallback, const char* format, Args... args) {
  if (bufferSize == 0) return false;
  // format is a parameter by design (printf-style wrapper), so GCC's
  // -Wformat-security sees a "non-literal format without arguments" for
  // calls with an empty argument pack. The arguments are checked at the
  // caller by -Wformat when the format is a literal; suppress only here.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
  const int written = std::snprintf(buffer, bufferSize, format, args...);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  if (written < 0 || static_cast<std::size_t>(written) >= bufferSize) {
    // Best-effort localized fallback. Copy it manually so a fallback longer
    // than the buffer cannot trigger -Wformat-truncation and so the result is
    // always NUL-terminated.
    if (fallback != nullptr) {
      const std::size_t length = std::strlen(fallback);
      const std::size_t copied = length < bufferSize - 1 ? length : bufferSize - 1;
      std::memcpy(buffer, fallback, copied);
      buffer[copied] = '\0';
    } else {
      buffer[0] = '\0';
    }
    return false;
  }
  return true;
}

// Checked narrowing of an integral value at a UI/platform boundary. Returns
// false (leaving `out` untouched) when the value cannot be represented in the
// target type, so callers can skip the item or use a safe fallback instead of
// wrapping or relying on implicit conversions.
template <typename Target, typename Source>
bool checkedCast(const Source value, Target& out) {
  static_assert(std::is_integral_v<Target>, "checkedCast target must be an integral type");
  static_assert(std::is_integral_v<Source>, "checkedCast source must be an integral type");
  if constexpr (std::is_signed_v<Target>) {
    if (std::is_signed_v<Source>) {
      if (value < static_cast<Source>(std::numeric_limits<Target>::min()) ||
          value > static_cast<Source>(std::numeric_limits<Target>::max())) {
        return false;
      }
    } else if (value > static_cast<Source>(std::numeric_limits<Target>::max())) {
      return false;
    }
  } else if (std::is_signed_v<Source>) {
    if (value < 0) return false;
    using UnsignedSource = std::make_unsigned_t<Source>;
    if (static_cast<UnsignedSource>(value) > static_cast<UnsignedSource>(std::numeric_limits<Target>::max())) {
      return false;
    }
  } else if (value > static_cast<Source>(std::numeric_limits<Target>::max())) {
    return false;
  }
  out = static_cast<Target>(value);
  return true;
}

// Convert a layout/metrics int to an int16_t inset. Geometry outside the
// int16 range indicates a broken theme or screen bounds; clamp rather than
// wrap so an inset can never become negative or inverted.
inline int16_t clampedInset(const int value) {
  if (value < std::numeric_limits<int16_t>::min()) return std::numeric_limits<int16_t>::min();
  if (value > std::numeric_limits<int16_t>::max()) return std::numeric_limits<int16_t>::max();
  return static_cast<int16_t>(value);
}

}  // namespace studypet
