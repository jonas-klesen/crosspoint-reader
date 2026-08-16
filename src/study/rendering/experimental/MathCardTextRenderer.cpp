#include "MathCardTextRenderer.h"

#if defined(STUDYPET_MATH_SPIKE)

#include <GfxRenderer.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../CardTextRenderer.h"
#include "MathLayout.h"
#include "fontIds.h"

namespace studypet::mathspike {
namespace {

constexpr std::size_t MAX_RUNS = 32;
constexpr int MATH_SCRIPT_FONT_ID = NOTOSANS_12_FONT_ID;

struct Run {
  enum class Kind : uint8_t { Plain, Math };
  Kind kind = Kind::Plain;
  std::size_t start = 0;
  std::size_t length = 0;
  bool display = false;
};

struct Pattern {
  uint8_t width = 0;
  uint8_t height = 0;
  uint8_t ascent = 0;
  std::array<uint8_t, 10> rows{};
};

bool escapedDollar(const std::string_view text, const std::size_t position) {
  std::size_t slashCount = 0;
  while (position > slashCount && text[position - slashCount - 1] == '\\') ++slashCount;
  return (slashCount & 1U) != 0;
}

bool hasUnclosedDelimiter(const std::string_view text) {
  std::size_t openLength = 0;
  std::size_t position = 0;
  while (position < text.size()) {
    if (text[position] != '$' || escapedDollar(text, position)) {
      ++position;
      continue;
    }
    if (openLength == 0) {
      openLength = position + 1 < text.size() && text[position + 1] == '$' ? 2 : 1;
      position += openLength;
    } else if (openLength == 2 && position + 1 < text.size() && text[position + 1] == '$') {
      openLength = 0;
      position += 2;
    } else if (openLength == 1) {
      openLength = 0;
      ++position;
    } else {
      ++position;
    }
  }
  return openLength != 0;
}

bool appendRun(std::array<Run, MAX_RUNS>& runs, std::size_t& count, const Run run) {
  if (count >= runs.size()) return false;
  runs[count++] = run;
  return true;
}

bool collectRuns(const std::string_view text, std::array<Run, MAX_RUNS>& runs, std::size_t& count) {
  count = 0;
  std::size_t plainStart = 0;
  std::size_t position = 0;
  while (position < text.size()) {
    if (text[position] != '$' || escapedDollar(text, position)) {
      ++position;
      continue;
    }

    const std::size_t delimiterLength = position + 1 < text.size() && text[position + 1] == '$' ? 2 : 1;
    std::size_t close = position + delimiterLength;
    while (close + delimiterLength <= text.size()) {
      if (text[close] == '$' && !escapedDollar(text, close) &&
          (delimiterLength == 1 || (close + 1 < text.size() && text[close + 1] == '$'))) {
        break;
      }
      ++close;
    }
    if (close + delimiterLength > text.size() || close == position + delimiterLength) return false;

    if (position > plainStart &&
        !appendRun(runs, count, Run{Run::Kind::Plain, plainStart, position - plainStart, false})) {
      return false;
    }
    if (!appendRun(runs, count,
                   Run{Run::Kind::Math, position + delimiterLength, close - position - delimiterLength,
                       delimiterLength == 2})) {
      return false;
    }
    position = close + delimiterLength;
    plainStart = position;
  }

  if (plainStart < text.size() &&
      !appendRun(runs, count, Run{Run::Kind::Plain, plainStart, text.size() - plainStart, false})) {
    return false;
  }
  return true;
}

bool copyText(const std::string_view source, std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1>& buffer) {
  if (source.size() > card_text_detail::MAX_RENDER_LINE_BYTES) return false;
  for (std::size_t i = 0; i < source.size(); ++i) buffer[i] = source[i];
  buffer[source.size()] = '\0';
  return true;
}

int plainWidth(const GfxRenderer& renderer, const int fontId, const std::string_view text,
               const EpdFontFamily::Style style) {
  std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
  if (!copyText(text, buffer)) return 1'000'000;
  return std::max(0, renderer.getTextWidth(fontId, buffer.data(), style));
}

int mixedWidth(const GfxRenderer& renderer, const int fontId, const std::string_view text,
               const EpdFontFamily::Style style) {
  std::array<Run, MAX_RUNS> runs{};
  std::size_t count = 0;
  if (!collectRuns(text, runs, count))
    return hasUnclosedDelimiter(text) ? 0 : plainWidth(renderer, fontId, text, style);
  int width = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const Run& run = runs[i];
    if (run.kind == Run::Kind::Plain) {
      width += plainWidth(renderer, fontId, text.substr(run.start, run.length), style);
      continue;
    }
    const MathLayout formula = parseMath(text.substr(run.start, run.length));
    if (!formula.valid()) return plainWidth(renderer, fontId, text, style);
    width += formula.width();
  }
  return width;
}

int mixedHeight(const std::string_view text, const int baseLineHeight) {
  std::array<Run, MAX_RUNS> runs{};
  std::size_t count = 0;
  if (!collectRuns(text, runs, count)) return baseLineHeight;

  int height = baseLineHeight;
  for (std::size_t i = 0; i < count; ++i) {
    if (runs[i].kind != Run::Kind::Math) continue;
    const MathLayout formula = parseMath(text.substr(runs[i].start, runs[i].length));
    if (formula.valid()) height = std::max(height, formula.height() + (runs[i].display ? 6 : 4));
  }
  return height;
}

Pattern patternFor(const MathSymbol symbol) {
  switch (symbol) {
    case MathSymbol::Alpha:
      return {5, 7, 7, {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}};
    case MathSymbol::Beta:
      return {5, 7, 7, {0b11110, 0b10001, 0b11110, 0b10001, 0b10001, 0b10001, 0b11110}};
    case MathSymbol::Gamma:
      return {5, 7, 7, {0b11111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000}};
    case MathSymbol::Delta:
      return {5, 7, 7, {0b00100, 0b01010, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001}};
    case MathSymbol::Epsilon:
      return {5, 7, 7, {0b01111, 0b10000, 0b10000, 0b01110, 0b10000, 0b10000, 0b01111}};
    case MathSymbol::Lambda:
      return {5, 7, 7, {0b00100, 0b01010, 0b01010, 0b10001, 0b10001, 0b10001, 0b10001}};
    case MathSymbol::Mu:
      return {5, 7, 7, {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}};
    case MathSymbol::Pi:
      return {5, 7, 7, {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}};
    case MathSymbol::Sigma:
      return {5, 7, 7, {0b11111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11111}};
    case MathSymbol::Theta:
      return {5, 7, 7, {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b01110}};
    case MathSymbol::Sum:
      return {
          7, 9, 9, {0b0111110, 0b0100000, 0b0100000, 0b0111100, 0b0000010, 0b0000010, 0b0100010, 0b0100010, 0b0111110}};
    case MathSymbol::Product:
      return {
          7, 9, 9, {0b1000001, 0b1100011, 0b1010101, 0b1010101, 0b1000001, 0b1000001, 0b1000001, 0b1000001, 0b1000001}};
    case MathSymbol::Integral:
      return {5, 9, 9, {0b00110, 0b01000, 0b01000, 0b01000, 0b01100, 0b01000, 0b01000, 0b01000, 0b01100}};
    case MathSymbol::Infinity:
      return {7, 7, 7, {0b0110010, 0b1001101, 0b1000101, 0b0110010, 0b1000101, 0b1001101, 0b0110010}};
    case MathSymbol::LessEqual:
      return {7, 7, 7, {0b0000010, 0b0001100, 0b0110000, 0b1100000, 0b0110000, 0b0001100, 0b0000010}};
    case MathSymbol::GreaterEqual:
      return {7, 7, 7, {0b0100000, 0b0011000, 0b0000110, 0b0000011, 0b0000110, 0b0011000, 0b0100000}};
    case MathSymbol::NotEqual:
      return {7, 7, 7, {0b0000001, 0b0000010, 0b1111111, 0b0001000, 0b0010000, 0b1111111, 0b0100000}};
    case MathSymbol::Approx:
      return {7, 7, 7, {0b0000000, 0b0110011, 0b1001100, 0b0000000, 0b0110011, 0b1001100, 0b0000000}};
    case MathSymbol::Arrow:
      return {7, 7, 7, {0b0001000, 0b0001000, 0b0001000, 0b1111111, 0b0001000, 0b0011100, 0b0001000}};
    default:
      return {};
  }
}

void drawPattern(GfxRenderer& renderer, const Pattern& pattern, const int x, const int top) {
  constexpr int scale = 2;
  for (int row = 0; row < pattern.height; ++row) {
    for (int col = 0; col < pattern.width; ++col) {
      if ((pattern.rows[static_cast<std::size_t>(row)] & (1U << (pattern.width - 1 - col))) == 0) continue;
      for (int dy = 0; dy < scale; ++dy) {
        for (int dx = 0; dx < scale; ++dx) renderer.drawPixel(x + col * scale + dx, top + row * scale + dy, true);
      }
    }
  }
}

void drawNode(GfxRenderer& renderer, const MathLayout& layout, const uint8_t index, const int x, const int baseline,
              const int fontId, const int scriptFontId, const MathMetrics& metrics, const uint8_t depth) {
  if (index == MathNode::NONE || index >= layout.nodeCount() || depth > MathLayout::MAX_NESTING + 3) return;
  const MathNode& current = layout.node(index);
  switch (current.kind) {
    case MathNode::Kind::Glyph: {
      if (current.symbol == MathSymbol::Space) return;
      const Pattern pattern = patternFor(current.symbol);
      if (pattern.width != 0) {
        drawPattern(renderer, pattern, x, baseline - pattern.ascent * 2);
      } else {
        char text[2] = {current.literal, '\0'};
        renderer.drawText(fontId, x, baseline - renderer.getFontAscenderSize(fontId), text, true);
      }
      return;
    }
    case MathNode::Kind::Row: {
      int cursor = x;
      for (uint8_t offset = 0; offset < current.childCount; ++offset) {
        if (offset != 0) cursor += metrics.atomGap;
        const uint8_t child = layout.child(index, offset);
        drawNode(renderer, layout, child, cursor, baseline, fontId, scriptFontId, metrics,
                 static_cast<uint8_t>(depth + 1));
        cursor += layout.node(child).width;
      }
      return;
    }
    case MathNode::Kind::Fraction: {
      const MathNode& numerator = layout.node(current.first);
      const MathNode& denominator = layout.node(current.second);
      const int top = baseline - current.ascent;
      const int numeratorHeight = numerator.ascent + numerator.descent;
      const int denominatorHeight = denominator.ascent + denominator.descent;
      const int numeratorX = x + (current.width - numerator.width) / 2;
      const int denominatorX = x + (current.width - denominator.width) / 2;
      drawNode(renderer, layout, current.first, numeratorX, top + numerator.ascent, scriptFontId, scriptFontId, metrics,
               static_cast<uint8_t>(depth + 1));
      const int ruleY = top + numeratorHeight + metrics.fractionGap;
      renderer.drawLine(x + 1, ruleY, x + current.width - 2, ruleY, true);
      const int denominatorTop = ruleY + metrics.fractionRule + metrics.fractionGap;
      drawNode(renderer, layout, current.second, denominatorX, denominatorTop + denominator.ascent, scriptFontId,
               scriptFontId, metrics, static_cast<uint8_t>(depth + 1));
      (void)denominatorHeight;
      return;
    }
    case MathNode::Kind::Sqrt: {
      const MathNode& radicand = layout.node(current.first);
      const int top = baseline - current.ascent;
      const int radicalRight = x + metrics.radicalWidth - 2;
      renderer.drawLine(x + 1, top + current.ascent - 3, x + 3, top + current.ascent - 1, true);
      renderer.drawLine(x + 3, top + current.ascent - 1, x + 5, top + 2, true);
      renderer.drawLine(x + 5, top + 2, radicalRight, top, true);
      renderer.drawLine(radicalRight, top, x + current.width - 1, top, true);
      drawNode(renderer, layout, current.first, x + metrics.radicalWidth, baseline, fontId, scriptFontId, metrics,
               static_cast<uint8_t>(depth + 1));
      (void)radicand;
      return;
    }
    case MathNode::Kind::Script: {
      const MathNode& base = layout.node(current.first);
      drawNode(renderer, layout, current.first, x, baseline, fontId, scriptFontId, metrics,
               static_cast<uint8_t>(depth + 1));
      const int scriptX = x + base.width;
      if (current.second != MathNode::NONE) {
        const MathNode& superscript = layout.node(current.second);
        drawNode(renderer, layout, current.second, scriptX, baseline - base.ascent - metrics.scriptGap, scriptFontId,
                 scriptFontId, metrics, static_cast<uint8_t>(depth + 1));
        (void)superscript;
      }
      if (current.third != MathNode::NONE) {
        drawNode(renderer, layout, current.third, scriptX,
                 baseline + base.descent + layout.node(current.third).ascent + metrics.scriptGap, scriptFontId,
                 scriptFontId, metrics, static_cast<uint8_t>(depth + 1));
      }
      return;
    }
  }
}

void drawLiteralLine(const GfxRenderer& renderer, const std::string_view line, const CardTextLine& layoutLine,
                     const int left, const int top, const int width, const int fontId, const int lineHeight,
                     const EpdFontFamily::Style style) {
  std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
  if (!copyText(line, buffer)) return;
  const int indent = std::max(0, std::min(layoutLine.indentPixels, std::max(0, width - 1)));
  const int availableWidth = std::max(1, width - indent);
  const char* drawLine = buffer.data();
  std::string_view truncatedView;
  if (layoutLine.ellipsize || renderer.getTextWidth(fontId, drawLine, style) > availableWidth) {
    const std::string truncated = renderer.truncatedText(fontId, drawLine, availableWidth, style);
    std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> copy{};
    if (!copyText(truncated, copy)) return;
    const int textWidth = renderer.getTextWidth(fontId, copy.data(), style);
    const int x = left + (layoutLine.leftAligned ? indent : std::max(0, (width - textWidth) / 2));
    renderer.drawText(fontId, x, top, copy.data(), true, style);
    return;
  }

  const int textWidth = renderer.getTextWidth(fontId, drawLine, style);
  int x = layoutLine.leftAligned ? indent : std::max(0, (width - textWidth) / 2);
  x = std::max(indent, std::min(x, width - std::max(0, textWidth)));
  renderer.drawText(fontId, left + x, top, drawLine, true, style);
  (void)lineHeight;
  (void)truncatedView;
}

}  // namespace

bool MathCardTextRenderer::containsMath(const std::string_view text) noexcept {
  std::array<Run, MAX_RUNS> runs{};
  std::size_t count = 0;
  if (!collectRuns(text, runs, count)) return false;
  for (std::size_t i = 0; i < count; ++i) {
    if (runs[i].kind == Run::Kind::Math) return true;
  }
  return false;
}

void MathCardTextRenderer::draw(const std::string_view text, const int left, const int top, const int width,
                                const int height, const int fontId, const int maxLines,
                                const EpdFontFamily::Style style) const {
  if (text.empty() || width <= 0 || height <= 0 || maxLines <= 0) return;
  const int baseLineHeight = renderer_.getLineHeight(fontId);
  if (baseLineHeight <= 0) return;

  const auto measure = [&](const char* line) { return mixedWidth(renderer_, fontId, std::string_view(line), style); };
  const CardTextLayout layout = layoutCardText(text, width, maxLines, measure);
  if (layout.count == 0) return;

  std::array<int, CARD_TEXT_MAX_LINES> lineHeights{};
  int blockHeight = 0;
  for (std::size_t index = 0; index < layout.count; ++index) {
    const CardTextLine& line = layout.lines[index];
    std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
    std::size_t outputLength = 0;
    if (!card_text_detail::expandRange(text, line.start, line.length, buffer, outputLength)) {
      lineHeights[index] = baseLineHeight;
    } else {
      lineHeights[index] = mixedHeight(std::string_view(buffer.data(), outputLength), baseLineHeight);
    }
    blockHeight += lineHeights[index];
  }

  int y = top + std::max(0, (height - blockHeight) / 2);
  for (std::size_t index = 0; index < layout.count; ++index) {
    const CardTextLine& line = layout.lines[index];
    std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
    std::size_t outputLength = 0;
    if (!card_text_detail::expandRange(text, line.start, line.length, buffer, outputLength)) {
      y += lineHeights[index];
      continue;
    }
    const std::string_view visualLine(buffer.data(), outputLength);
    std::array<Run, MAX_RUNS> runs{};
    std::size_t count = 0;
    if (!collectRuns(visualLine, runs, count)) {
      drawLiteralLine(renderer_, visualLine, line, left, y, width, fontId, lineHeights[index], style);
      y += lineHeights[index];
      continue;
    }

    const int indent = std::max(0, std::min(line.indentPixels, std::max(0, width - 1)));
    const int availableWidth = std::max(1, width - indent);
    const int textWidth = mixedWidth(renderer_, fontId, visualLine, style);
    if (textWidth > availableWidth || line.ellipsize) {
      drawLiteralLine(renderer_, visualLine, line, left, y, width, fontId, lineHeights[index], style);
      y += lineHeights[index];
      continue;
    }

    int x = line.leftAligned ? indent : std::max(0, (width - textWidth) / 2);
    x = std::max(indent, std::min(x, width - std::max(0, textWidth)));
    const int baseline = y + renderer_.getFontAscenderSize(fontId);
    for (std::size_t runIndex = 0; runIndex < count; ++runIndex) {
      const Run& run = runs[runIndex];
      if (run.kind == Run::Kind::Plain) {
        std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> plain{};
        if (!copyText(visualLine.substr(run.start, run.length), plain)) continue;
        renderer_.drawText(fontId, left + x, y, plain.data(), true, style);
        x += renderer_.getTextWidth(fontId, plain.data(), style);
        continue;
      }

      const MathLayout formula = parseMath(visualLine.substr(run.start, run.length));
      if (!formula.valid()) {
        drawLiteralLine(renderer_, visualLine, line, left, y, width, fontId, lineHeights[index], style);
        break;
      }
      drawNode(renderer_, formula, formula.root(), left + x, baseline, fontId, MATH_SCRIPT_FONT_ID, MathMetrics{}, 0);
      x += formula.width();
    }
    y += lineHeights[index];
  }
}

}  // namespace studypet::mathspike

#endif  // STUDYPET_MATH_SPIKE
