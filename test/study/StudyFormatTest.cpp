#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "StudyFormat.h"

namespace {

using studypet::checkedCast;
using studypet::clampedInset;
using studypet::safeFormat;

TEST(StudyFormat, FormatsWhenBufferFits) {
  char buffer[32]{};
  EXPECT_TRUE(safeFormat(buffer, sizeof(buffer), "fallback", "%zu cards", std::size_t{12}));
  EXPECT_STREQ(buffer, "12 cards");
}

TEST(StudyFormat, ReportsExactFitAsSuccess) {
  char buffer[9]{};
  EXPECT_TRUE(safeFormat(buffer, sizeof(buffer), "fallback", "%zu", std::size_t{12345678}));
  EXPECT_STREQ(buffer, "12345678");
}

TEST(StudyFormat, AppliesFallbackOnTruncation) {
  // A runtime (volatile) value keeps the compiler from folding the constant so
  // it cannot prove the deliberate truncation and warn about it.
  char buffer[8]{};
  volatile std::size_t value = 123456789;
  EXPECT_FALSE(safeFormat(buffer, sizeof(buffer), "fallback", "%zu", value));
  // The fallback itself is truncated to the buffer's usable capacity.
  EXPECT_STREQ(buffer, "fallbac");
}

TEST(StudyFormat, AppliesFallbackOnFormattingError) {
  // A runtime format string with an invalid conversion makes snprintf return a
  // negative value; the extra argument keeps -Wformat-security quiet.
  const char* format = "%q";
  char buffer[32]{};
  EXPECT_FALSE(safeFormat(buffer, sizeof(buffer), "fallback", format, 1));
  EXPECT_STREQ(buffer, "fallback");
}

TEST(StudyFormat, NullFallbackEmptiesBuffer) {
  char buffer[8]{};
  volatile std::size_t value = 123456789;
  EXPECT_FALSE(safeFormat(buffer, sizeof(buffer), nullptr, "%zu", value));
  EXPECT_STREQ(buffer, "");
}

TEST(StudyFormat, ZeroSizedBufferFails) { EXPECT_FALSE(safeFormat(static_cast<char*>(nullptr), 0, "fallback", "x")); }

TEST(StudyFormat, CheckedCastHandlesSignedAndUnsignedBounds) {
  int16_t out = 0;
  EXPECT_TRUE(checkedCast(std::size_t{7}, out));
  EXPECT_EQ(out, 7);
  EXPECT_TRUE(checkedCast(std::numeric_limits<int16_t>::max(), out));
  EXPECT_EQ(out, std::numeric_limits<int16_t>::max());
  EXPECT_FALSE(checkedCast(static_cast<std::size_t>(std::numeric_limits<int16_t>::max()) + 1, out));
  EXPECT_FALSE(checkedCast(std::numeric_limits<int>::min(), out));

  uint16_t uout = 0;
  EXPECT_TRUE(checkedCast(std::size_t{65535}, uout));
  EXPECT_EQ(uout, 65535);
  EXPECT_FALSE(checkedCast(std::size_t{65536}, uout));
  EXPECT_FALSE(checkedCast(-1, uout));
}

TEST(StudyFormat, ClampedInsetClampsInsteadOfWrapping) {
  EXPECT_EQ(clampedInset(0), 0);
  EXPECT_EQ(clampedInset(100), 100);
  EXPECT_EQ(clampedInset(std::numeric_limits<int>::max()), std::numeric_limits<int16_t>::max());
  EXPECT_EQ(clampedInset(std::numeric_limits<int>::min()), std::numeric_limits<int16_t>::min());
}

}  // namespace
