#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>

#include "CardTextRenderer.h"

namespace {

int fixedWidth(const char* text) {
  int width = 0;
  for (std::size_t index = 0; text[index] != '\0'; ++index) {
    if ((static_cast<unsigned char>(text[index]) & 0xC0U) != 0x80U) width += 5;
  }
  return width;
}

TEST(CardTextLayout, PreservesExplicitAndBlankLines) {
  const std::string_view text = "first\n\nthird";
  const studypet::CardTextLayout layout = studypet::layoutCardText(text, 200, 8, fixedWidth);

  ASSERT_EQ(layout.count, 3u);
  EXPECT_EQ(layout.lines[0].length, 5u);
  EXPECT_EQ(layout.lines[1].length, 0u);
  EXPECT_EQ(layout.lines[2].start, 7u);
  EXPECT_FALSE(layout.clipped);
}

TEST(CardTextLayout, WrapsAtWordsAndClipsAtLineLimit) {
  const studypet::CardTextLayout layout = studypet::layoutCardText("one two three four", 35, 2, fixedWidth);

  ASSERT_EQ(layout.count, 2u);
  EXPECT_EQ(layout.lines[0].length, 3u);
  EXPECT_TRUE(layout.lines[1].ellipsize);
  EXPECT_TRUE(layout.clipped);
}

TEST(CardTextLayout, TabsUseFourSpaceStops) {
  const studypet::CardTextLayout layout = studypet::layoutCardText("a\tb", 20, 4, fixedWidth);

  ASSERT_EQ(layout.count, 2u);
  EXPECT_EQ(layout.lines[0].start, 0u);
  EXPECT_EQ(layout.lines[0].length, 1u);
  EXPECT_EQ(layout.lines[1].start, 2u);
  EXPECT_EQ(layout.lines[1].length, 1u);
}

TEST(CardTextLayout, BulletsUseHangingIndentation) {
  const studypet::CardTextLayout layout = studypet::layoutCardText("- one two three", 50, 4, fixedWidth);

  ASSERT_EQ(layout.count, 2u);
  EXPECT_TRUE(layout.lines[0].leftAligned);
  EXPECT_EQ(layout.lines[0].indentPixels, 0);
  EXPECT_TRUE(layout.lines[1].leftAligned);
  EXPECT_EQ(layout.lines[1].indentPixels, 10);
  EXPECT_EQ(layout.lines[1].start, 10u);
}

TEST(CardTextLayout, NumberedPrefixesAlsoHang) {
  const studypet::CardTextLayout layout = studypet::layoutCardText("10. alpha beta", 50, 4, fixedWidth);

  ASSERT_EQ(layout.count, 2u);
  EXPECT_TRUE(layout.lines[1].leftAligned);
  EXPECT_EQ(layout.lines[1].indentPixels, 20);
}

TEST(CardTextLayout, EmptyAndUtf8TextAreSafe) {
  EXPECT_EQ(studypet::layoutCardText("", 50, 4, fixedWidth).count, 0u);

  const studypet::CardTextLayout layout = studypet::layoutCardText("αβγ", 5, 1, fixedWidth);
  ASSERT_EQ(layout.count, 1u);
  EXPECT_EQ(layout.lines[0].length, 2u);
  EXPECT_TRUE(layout.lines[0].ellipsize);
  EXPECT_TRUE(layout.clipped);
}

TEST(CardTextLayout, NarrowBoundsStillMakeProgress) {
  const studypet::CardTextLayout layout = studypet::layoutCardText("longword", 1, 7, fixedWidth);

  EXPECT_EQ(layout.count, 7u);
  EXPECT_TRUE(layout.clipped);
}

}  // namespace
