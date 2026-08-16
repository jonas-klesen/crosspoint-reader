#pragma once

#include <cstddef>
#include <cstdint>

namespace studycore {

inline constexpr std::size_t SPM_HEADER_SIZE = 8;
inline constexpr uint16_t MAX_MATH_ASSET_WIDTH = 488;
inline constexpr uint16_t MAX_MATH_ASSET_HEIGHT = 120;
inline constexpr char SPM_MAGIC[4] = {'S', 'P', 'M', '1'};

inline constexpr std::size_t spmRowBytes(const uint16_t width) noexcept {
  return (static_cast<std::size_t>(width) + 7U) / 8U;
}

inline constexpr uint64_t spmExpectedFileSize(const uint16_t width, const uint16_t height) noexcept {
  return static_cast<uint64_t>(SPM_HEADER_SIZE) +
         static_cast<uint64_t>(spmRowBytes(width)) * static_cast<uint64_t>(height);
}

enum class SpmValidationError : uint8_t {
  None,
  ShortHeader,
  InvalidMagic,
  ZeroWidth,
  ZeroHeight,
  WidthTooLarge,
  HeightTooLarge,
  SizeMismatch,
};

struct SpmDimensions {
  uint16_t width = 0;
  uint16_t height = 0;
  std::size_t rowBytes = 0;
  uint64_t expectedFileSize = 0;
};

inline SpmValidationError validateSpmHeader(const uint8_t* header, const std::size_t headerSize,
                                            const uint64_t fileSize, SpmDimensions& dimensions) noexcept {
  dimensions = {};
  if (header == nullptr || headerSize < SPM_HEADER_SIZE) return SpmValidationError::ShortHeader;
  for (std::size_t index = 0; index < sizeof(SPM_MAGIC); ++index) {
    if (header[index] != static_cast<uint8_t>(SPM_MAGIC[index])) return SpmValidationError::InvalidMagic;
  }

  const uint16_t width = static_cast<uint16_t>(header[4]) | (static_cast<uint16_t>(header[5]) << 8U);
  const uint16_t height = static_cast<uint16_t>(header[6]) | (static_cast<uint16_t>(header[7]) << 8U);
  if (width == 0) return SpmValidationError::ZeroWidth;
  if (height == 0) return SpmValidationError::ZeroHeight;
  if (width > MAX_MATH_ASSET_WIDTH) return SpmValidationError::WidthTooLarge;
  if (height > MAX_MATH_ASSET_HEIGHT) return SpmValidationError::HeightTooLarge;

  dimensions.width = width;
  dimensions.height = height;
  dimensions.rowBytes = spmRowBytes(width);
  dimensions.expectedFileSize = spmExpectedFileSize(width, height);
  if (fileSize != dimensions.expectedFileSize) return SpmValidationError::SizeMismatch;
  return SpmValidationError::None;
}

}  // namespace studycore
