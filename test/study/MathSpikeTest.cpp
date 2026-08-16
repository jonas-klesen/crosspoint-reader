#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "MathLayout.h"

namespace {

using studypet::mathspike::MathLayout;
using studypet::mathspike::MathNode;
using studypet::mathspike::parseMath;

const MathNode& firstChild(const MathLayout& layout) { return layout.node(layout.child(layout.root(), 0)); }

bool containsFraction(const MathLayout& layout, const uint8_t index, const int depth = 0) {
  if (index == MathNode::NONE || depth > 12) return false;
  const MathNode& node = layout.node(index);
  if (node.kind == MathNode::Kind::Fraction) return true;
  if (node.kind == MathNode::Kind::Row) {
    for (uint8_t offset = 0; offset < node.childCount; ++offset) {
      if (containsFraction(layout, layout.child(index, offset), depth + 1)) {
        return true;
      }
    }
  }
  if (node.kind == MathNode::Kind::Sqrt || node.kind == MathNode::Kind::Script) {
    return containsFraction(layout, node.first, depth + 1) || containsFraction(layout, node.second, depth + 1) ||
           containsFraction(layout, node.third, depth + 1);
  }
  if (node.kind == MathNode::Kind::Fraction) {
    return containsFraction(layout, node.first, depth + 1) || containsFraction(layout, node.second, depth + 1);
  }
  return false;
}

TEST(MathSpikeParser, ParsesLiteralAndScripts) {
  const MathLayout layout = parseMath("x^2+y_1=z^2");

  ASSERT_TRUE(layout.valid());
  EXPECT_GT(layout.width(), 0);
  EXPECT_GT(layout.height(), 0);
  EXPECT_EQ(layout.width(), parseMath("x^2+y_1=z^2").width());
}

TEST(MathSpikeParser, ParsesNestedGroupsAndFractions) {
  const MathLayout layout = parseMath("\\frac{1}{1+\\frac{1}{x}}");

  ASSERT_TRUE(layout.valid());
  const MathNode& outer = firstChild(layout);
  ASSERT_EQ(outer.kind, MathNode::Kind::Fraction);
  EXPECT_GE(outer.width, layout.node(outer.first).width);
  EXPECT_GE(outer.width, layout.node(outer.second).width);
  EXPECT_EQ(layout.node(outer.second).kind, MathNode::Kind::Row);
  EXPECT_TRUE(containsFraction(layout, outer.second));
}

TEST(MathSpikeParser, ParsesRootGreekAndOperators) {
  const MathLayout layout = parseMath("\\sqrt{x^2+1}+\\alpha+\\beta=\\gamma+\\sum_{i=1}^{n}i+\\int_0^1x");

  ASSERT_TRUE(layout.valid());
  EXPECT_GT(layout.width(), 40);
  EXPECT_GT(layout.ascent(), layout.descent());
}

TEST(MathSpikeParser, RejectsUnknownAndMalformedCommands) {
  EXPECT_FALSE(parseMath("\\unknown{x}").valid());
  EXPECT_FALSE(parseMath("\\frac{a}{").valid());
  EXPECT_FALSE(parseMath("\\frac{a}{b}} ").valid());
  EXPECT_FALSE(parseMath("{}").valid());
  EXPECT_FALSE(parseMath("x^^2").valid());
}

TEST(MathSpikeParser, EnforcesNestingAndNodeBounds) {
  std::string nested;
  for (int i = 0; i < 9; ++i) nested += '{';
  nested += 'x';
  for (int i = 0; i < 9; ++i) nested += '}';
  EXPECT_FALSE(parseMath(nested).valid());

  const std::string validRow(30, 'x');
  EXPECT_TRUE(parseMath(validRow).valid());
  const std::string tooManyNodes(100, 'x');
  EXPECT_FALSE(parseMath(tooManyNodes).valid());

  const std::string tooLong(MathLayout::MAX_SOURCE_BYTES + 1, 'x');
  EXPECT_FALSE(parseMath(tooLong).valid());
}

TEST(MathSpikeParser, RejectsUtf8InsideMathWithoutCrashing) {
  EXPECT_FALSE(parseMath("\xCE\xB1+1").valid());
  EXPECT_FALSE(parseMath("\xF0\x9F\xA7\xAE").valid());
}

TEST(MathSpikeLayout, FractionBoundsContainBothChildren) {
  const MathLayout layout = parseMath("\\frac{a+b}{c}");

  ASSERT_TRUE(layout.valid());
  const MathNode& fraction = firstChild(layout);
  ASSERT_EQ(fraction.kind, MathNode::Kind::Fraction);
  EXPECT_GE(fraction.width, layout.node(fraction.first).width);
  EXPECT_GE(fraction.width, layout.node(fraction.second).width);
  EXPECT_GT(fraction.ascent, layout.node(fraction.first).ascent);
  EXPECT_GT(fraction.descent, layout.node(fraction.second).descent);
}

TEST(MathSpikeLayout, ScriptsExtendAboveAndBelowBase) {
  const MathLayout layout = parseMath("a_n^2");

  ASSERT_TRUE(layout.valid());
  const MathNode& script = firstChild(layout);
  ASSERT_EQ(script.kind, MathNode::Kind::Script);
  EXPECT_GT(script.ascent, layout.node(script.first).ascent);
  EXPECT_GE(script.descent, layout.node(script.third).descent);
}

TEST(MathSpikeLayout, SqrtContainsRadicand) {
  const MathLayout layout = parseMath("\\sqrt{x^2+1}");

  ASSERT_TRUE(layout.valid());
  const MathNode& root = firstChild(layout);
  ASSERT_EQ(root.kind, MathNode::Kind::Sqrt);
  EXPECT_GT(root.width, layout.node(root.first).width);
  EXPECT_GE(root.ascent, layout.node(root.first).ascent);
}

}  // namespace
