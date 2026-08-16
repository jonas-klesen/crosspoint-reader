#include "MathAssetRenderer.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "MathAssetFormat.h"
#include "MathAssetKey.h"

namespace studypet {
namespace {

constexpr char MODULE_NAME[] = "MATH";
constexpr std::size_t ASSET_PATH_BUFFER_SIZE = 64;
constexpr std::size_t SPM_ROW_BUFFER_SIZE = (static_cast<std::size_t>(studycore::MAX_MATH_ASSET_WIDTH) + 7U) / 8U;
constexpr std::size_t SPM_HEADER_SIZE = studycore::SPM_HEADER_SIZE;

MathAssetStatus statusFor(const studycore::SpmValidationError error) {
  switch (error) {
    case studycore::SpmValidationError::None:
      return MathAssetStatus::Valid;
    case studycore::SpmValidationError::ShortHeader:
      return MathAssetStatus::ReadFailed;
    case studycore::SpmValidationError::InvalidMagic:
      return MathAssetStatus::InvalidMagic;
    case studycore::SpmValidationError::ZeroWidth:
      return MathAssetStatus::ZeroWidth;
    case studycore::SpmValidationError::ZeroHeight:
      return MathAssetStatus::ZeroHeight;
    case studycore::SpmValidationError::WidthTooLarge:
      return MathAssetStatus::WidthTooLarge;
    case studycore::SpmValidationError::HeightTooLarge:
      return MathAssetStatus::HeightTooLarge;
    case studycore::SpmValidationError::SizeMismatch:
      return MathAssetStatus::SizeMismatch;
  }
  return MathAssetStatus::ReadFailed;
}

bool buildPath(const std::string_view formula, std::array<char, ASSET_PATH_BUFFER_SIZE>& path) {
  return studycore::formatMathAssetPath(studycore::mathAssetKey(formula), path.data(), path.size());
}

}  // namespace

MathAssetInfo MathAssetRenderer::inspect(const std::string_view formula) const {
  std::array<char, ASSET_PATH_BUFFER_SIZE> path{};
  if (!buildPath(formula, path)) return {MathAssetStatus::InvalidPath, 0, 0};

  HalFile file;
  if (!Storage.openFileForRead(MODULE_NAME, path.data(), file) || !file) {
    return {MathAssetStatus::Missing, 0, 0};
  }
  if (file.isDirectory()) {
    file.close();
    return {MathAssetStatus::ReadFailed, 0, 0};
  }

  const uint64_t fileSize = file.fileSize64();
  std::array<uint8_t, SPM_HEADER_SIZE> header{};
  const int bytesRead = file.read(header.data(), header.size());
  if (bytesRead != static_cast<int>(header.size())) {
    file.close();
    return {MathAssetStatus::ReadFailed, 0, 0};
  }

  studycore::SpmDimensions dimensions;
  const auto error = studycore::validateSpmHeader(header.data(), header.size(), fileSize, dimensions);
  file.close();
  if (error != studycore::SpmValidationError::None) {
    return {statusFor(error), dimensions.width, dimensions.height};
  }
  return {MathAssetStatus::Valid, dimensions.width, dimensions.height};
}

bool MathAssetRenderer::draw(const std::string_view formula, const int x, const int y) const {
  const MathAssetInfo info = inspect(formula);
  if (!info.valid()) {
    LOG_DBG("StudyMath", "Draw skipped status=%u", static_cast<unsigned>(info.status));
    return false;
  }

  const int screenWidth = renderer_.getScreenWidth();
  const int screenHeight = renderer_.getScreenHeight();
  if (x < 0 || y < 0 || x > screenWidth || y > screenHeight || static_cast<int>(info.width) > screenWidth - x ||
      static_cast<int>(info.height) > screenHeight - y) {
    LOG_DBG("StudyMath", "Draw out of bounds x=%d y=%d width=%u height=%u screen=%dx%d", x, y,
            static_cast<unsigned>(info.width), static_cast<unsigned>(info.height), screenWidth, screenHeight);
    return false;
  }

  std::array<char, ASSET_PATH_BUFFER_SIZE> path{};
  if (!buildPath(formula, path)) {
    LOG_DBG("StudyMath", "Draw path formatting failed");
    return false;
  }
  HalFile file;
  if (!Storage.openFileForRead(MODULE_NAME, path.data(), file) || !file) {
    LOG_DBG("StudyMath", "Draw open failed path=%s", path.data());
    return false;
  }

  const uint64_t fileSize = file.fileSize64();
  std::array<uint8_t, SPM_HEADER_SIZE> header{};
  if (file.read(header.data(), header.size()) != static_cast<int>(header.size())) {
    LOG_DBG("StudyMath", "Draw header read failed");
    file.close();
    return false;
  }
  studycore::SpmDimensions dimensions;
  if (studycore::validateSpmHeader(header.data(), header.size(), fileSize, dimensions) !=
      studycore::SpmValidationError::None) {
    LOG_DBG("StudyMath", "Draw header validation failed");
    file.close();
    return false;
  }
  if (!file.seek(SPM_HEADER_SIZE)) {
    LOG_DBG("StudyMath", "Draw seek failed");
    file.close();
    return false;
  }

  std::array<uint8_t, SPM_ROW_BUFFER_SIZE> row{};
  for (uint16_t rowIndex = 0; rowIndex < dimensions.height; ++rowIndex) {
    if (file.read(row.data(), dimensions.rowBytes) != static_cast<int>(dimensions.rowBytes)) {
      LOG_DBG("StudyMath", "Draw row read failed row=%u", static_cast<unsigned>(rowIndex));
      file.close();
      return false;
    }
    for (uint16_t column = 0; column < dimensions.width; ++column) {
      const uint8_t mask = static_cast<uint8_t>(0x80U >> (column % 8U));
      const bool ink = (row[column / 8U] & mask) != 0;
      if (ink) renderer_.drawPixel(x + column, y + rowIndex, true);
    }
  }
  file.close();
  return true;
}

}  // namespace studypet
