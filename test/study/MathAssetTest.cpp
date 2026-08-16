#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "MathAssetFormat.h"
#include "MathAssetKey.h"

namespace {

using studycore::extractMathFormula;
using studycore::formatMathAssetPath;
using studycore::mathAssetKey;
using studycore::SpmDimensions;
using studycore::SpmValidationError;
using studycore::validateSpmHeader;

std::array<uint8_t, studycore::SPM_HEADER_SIZE> header(const uint16_t width, const uint16_t height) {
  return {'S',
          'P',
          'M',
          '1',
          static_cast<uint8_t>(width & 0xFFU),
          static_cast<uint8_t>(width >> 8U),
          static_cast<uint8_t>(height & 0xFFU),
          static_cast<uint8_t>(height >> 8U)};
}

TEST(MathAssetKey, MatchesVersionedFnv1aVectors) {
  EXPECT_EQ(mathAssetKey(""), 0x1ff9c61977ba185aULL);
  EXPECT_EQ(mathAssetKey("x^2"), 0xbcacbd19eb695e4eULL);
  EXPECT_EQ(mathAssetKey("E = mc^2"), 0xd94c4f7e01bc9c32ULL);
  EXPECT_EQ(mathAssetKey(R"(\frac{1}{2}mv^2)"), 0x30de68743d73e9a0ULL);
  EXPECT_EQ(mathAssetKey(R"(\alpha + \beta)"), 0x77f0a20756aa1aa9ULL);
  EXPECT_EQ(mathAssetKey(R"(\sum_{i=1}^{n} i)"), 0x8c658cf65b641816ULL);
  EXPECT_EQ(mathAssetKey("π = 3.14"), 0xecf58baceaf1622aULL);
}

TEST(MathAssetKey, FormatsStableGlobalPath) {
  char path[64]{};
  ASSERT_TRUE(formatMathAssetPath(0x30de68743d73e9a0ULL, path, sizeof(path)));
  EXPECT_STREQ(path, "/study/assets/math/v1/30de68743d73e9a0.spm");
  EXPECT_FALSE(formatMathAssetPath(0, path, 0));
}
TEST(MathBlockRecognition, RequiresOneStandaloneNonemptyLine) {
  const std::array<std::string_view, 3> recognized{"$$x^2$$", R"($$\frac{a}{b}$$)", "$$ E = mc^2 $$"};
  for (const auto line : recognized) {
    std::string_view formula;
    ASSERT_TRUE(extractMathFormula(line, formula));
    EXPECT_FALSE(formula.empty());
  }

  const std::array<std::string_view, 8> literal{"$x^2$", "Text $$x^2$$", "$$x^2$$ text", "$$$$",
                                                "$",     "$$",           "Price: $10",   "$$\nx^2$$"};
  for (const auto line : literal) {
    std::string_view formula;
    EXPECT_FALSE(extractMathFormula(line, formula)) << line;
  }
}

TEST(SpmDecoder, AcceptsValidDimensionsAndExactPayload) {
  for (const auto dimensions : std::array<std::pair<uint16_t, uint16_t>, 4>{
           {{1, 1}, {9, 2}, {studycore::MAX_MATH_ASSET_WIDTH, 1}, {32, studycore::MAX_MATH_ASSET_HEIGHT}}}) {
    const auto bytes = header(dimensions.first, dimensions.second);
    SpmDimensions parsed;
    EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(),
                                studycore::spmExpectedFileSize(dimensions.first, dimensions.second), parsed),
              SpmValidationError::None);
    EXPECT_EQ(parsed.width, dimensions.first);
    EXPECT_EQ(parsed.height, dimensions.second);
  }
}

TEST(SpmDecoder, RejectsInvalidHeaderDimensionsAndSize) {
  auto bytes = header(1, 1);
  SpmDimensions parsed;
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 8, parsed), SpmValidationError::SizeMismatch);
  EXPECT_EQ(validateSpmHeader(bytes.data(), 7, 9, parsed), SpmValidationError::ShortHeader);
  bytes[0] = 'X';
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 9, parsed), SpmValidationError::InvalidMagic);

  bytes = header(0, 1);
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 9, parsed), SpmValidationError::ZeroWidth);
  bytes = header(1, 0);
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 9, parsed), SpmValidationError::ZeroHeight);
  bytes = header(static_cast<uint16_t>(studycore::MAX_MATH_ASSET_WIDTH + 1), 1);
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 9, parsed), SpmValidationError::WidthTooLarge);
  bytes = header(1, static_cast<uint16_t>(studycore::MAX_MATH_ASSET_HEIGHT + 1));
  EXPECT_EQ(validateSpmHeader(bytes.data(), bytes.size(), 9, parsed), SpmValidationError::HeightTooLarge);
}

}  // namespace
