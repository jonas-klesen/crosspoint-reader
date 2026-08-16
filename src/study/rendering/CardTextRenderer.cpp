#include "CardTextRenderer.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "MathAssetKey.h"
#include "MathAssetRenderer.h"

namespace studypet {
namespace {

constexpr int MATH_BLOCK_TOP_GAP = 3;
constexpr int MATH_BLOCK_BOTTOM_GAP = 3;

enum class CardTextBlockKind : uint8_t { Text, Math };

struct CardTextBlock {
  CardTextBlockKind kind = CardTextBlockKind::Text;
  CardTextLine line{};
  std::size_t formulaStart = 0;
  std::size_t formulaLength = 0;
  int width = 0;
  int height = 0;
  int blockHeight = 0;
  int lineSlots = 1;
};

struct CardTextBlockLayout {
  std::array<CardTextBlock, CARD_TEXT_MAX_LINES> blocks{};
  std::size_t count = 0;
  int totalHeight = 0;
  bool clipped = false;
};

bool containsStandaloneMath(const std::string_view text) {
  std::size_t logicalStart = 0;
  while (logicalStart <= text.size()) {
    const std::size_t newline = text.find('\n', logicalStart);
    const std::size_t logicalEnd = newline == std::string_view::npos ? text.size() : newline;
    std::string_view formula;
    if (studycore::extractMathFormula(text.substr(logicalStart, logicalEnd - logicalStart), formula)) return true;
    if (newline == std::string_view::npos) break;
    logicalStart = logicalEnd + 1;
  }
  return false;
}

CardTextBlockLayout layoutCardTextBlocks(const std::string_view text, const int maxWidth, const int height,
                                         const int fontId, const int maxLines, const EpdFontFamily::Style style,
                                         const MathAssetRenderer& mathRenderer, const GfxRenderer& renderer) {
  CardTextBlockLayout result;
  if (text.empty() || maxWidth <= 0 || height <= 0 || maxLines <= 0) return result;

  const int lineHeight = renderer.getLineHeight(fontId);
  if (lineHeight <= 0) return result;
  const std::size_t lineLimit = std::min<std::size_t>(CARD_TEXT_MAX_LINES, static_cast<std::size_t>(maxLines));
  std::size_t usedSlots = 0;
  const auto measure = [&](const char* line) { return renderer.getTextWidth(fontId, line, style); };

  auto appendText = [&](const std::string_view source, const std::size_t baseStart) {
    if (usedSlots >= lineLimit || result.count >= result.blocks.size()) {
      result.clipped = true;
      return false;
    }
    const CardTextLayout plain = layoutCardText(source, maxWidth, static_cast<int>(lineLimit - usedSlots), measure);
    for (std::size_t index = 0; index < plain.count; ++index) {
      if (usedSlots >= lineLimit || result.count >= result.blocks.size()) {
        result.clipped = true;
        return false;
      }
      CardTextBlock block;
      block.kind = CardTextBlockKind::Text;
      block.line = plain.lines[index];
      block.line.start += baseStart;
      block.blockHeight = lineHeight;
      result.blocks[result.count++] = block;
      result.totalHeight += lineHeight;
      ++usedSlots;
    }
    if (plain.clipped) {
      result.clipped = true;
      return false;
    }
    return true;
  };

  std::size_t logicalStart = 0;
  while (logicalStart <= text.size()) {
    const std::size_t newline = text.find('\n', logicalStart);
    const std::size_t logicalEnd = newline == std::string_view::npos ? text.size() : newline;
    const std::string_view line = text.substr(logicalStart, logicalEnd - logicalStart);
    std::string_view formula;
    const bool isMath = studycore::extractMathFormula(line, formula);
    bool appended = false;
    if (isMath) {
      const MathAssetInfo info = mathRenderer.inspect(formula);
      if (!info.valid()) {
        LOG_DBG("StudyMath", "Asset fallback status=%u formula=%.*s", static_cast<unsigned>(info.status),
                static_cast<int>(std::min<std::size_t>(formula.size(), 96)), formula.data());
      }
      const int blockHeight = MATH_BLOCK_TOP_GAP + static_cast<int>(info.height) + MATH_BLOCK_BOTTOM_GAP;
      const int slots = std::max(1, (blockHeight + lineHeight - 1) / lineHeight);
      if (info.valid() && info.width <= maxWidth && usedSlots + static_cast<std::size_t>(slots) <= lineLimit &&
          result.totalHeight + blockHeight <= height && result.count < result.blocks.size()) {
        CardTextBlock block;
        block.kind = CardTextBlockKind::Math;
        block.formulaStart = logicalStart + 2;
        block.formulaLength = formula.size();
        block.width = info.width;
        block.height = info.height;
        block.blockHeight = blockHeight;
        block.lineSlots = slots;
        result.blocks[result.count++] = block;
        result.totalHeight += blockHeight;
        usedSlots += static_cast<std::size_t>(slots);
        appended = true;
      }
      if (!appended) {
        LOG_DBG("StudyMath", "Text fallback status=%u width=%u max=%d height=%u used=%zu limit=%zu total=%d area=%d",
                static_cast<unsigned>(info.status), static_cast<unsigned>(info.width), maxWidth,
                static_cast<unsigned>(info.height), usedSlots, lineLimit, result.totalHeight, height);
        appended = appendText(formula, logicalStart + 2);
      }
    } else {
      appended = appendText(line, logicalStart);
    }
    if (!appended) break;

    if (newline == std::string_view::npos) break;
    logicalStart = logicalEnd + 1;
    if (logicalStart <= text.size() && usedSlots >= lineLimit) {
      result.clipped = true;
      break;
    }
  }
  return result;
}

void drawTextBlock(const GfxRenderer& renderer, const std::string_view source, const CardTextLine& line, const int left,
                   const int width, const int fontId, const EpdFontFamily::Style style, const int y) {
  std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
  std::size_t outputLength = 0;
  if (!card_text_detail::expandRange(source, line.start, line.length, buffer, outputLength)) return;

  const int indent = std::max(0, std::min(line.indentPixels, std::max(0, width - 1)));
  const int availableWidth = std::max(1, width - indent);
  std::string truncated;
  const char* drawLine = buffer.data();
  int textWidth = renderer.getTextWidth(fontId, drawLine, style);
  if (line.ellipsize || textWidth > availableWidth) {
    truncated = renderer.truncatedText(fontId, drawLine, availableWidth, style);
    drawLine = truncated.c_str();
    textWidth = renderer.getTextWidth(fontId, drawLine, style);
  }

  int x = left + indent;
  if (!line.leftAligned) x = left + std::max(0, (width - textWidth) / 2);
  x = std::max(left, std::min(x, left + width - std::max(0, textWidth)));
  renderer.drawText(fontId, x, y, drawLine, true, style);
}

}  // namespace

void CardTextRenderer::draw(const std::string_view text, const int left, const int top, const int width,
                            const int height, const int fontId, const int maxLines,
                            const EpdFontFamily::Style style) const {
  if (text.empty() || width <= 0 || height <= 0 || maxLines <= 0) return;

  const int lineHeight = renderer.getLineHeight(fontId);
  if (lineHeight <= 0) return;

  if (!containsStandaloneMath(text)) {
    const auto measure = [&](const char* line) { return renderer.getTextWidth(fontId, line, style); };
    const CardTextLayout layout = layoutCardText(text, width, maxLines, measure);
    if (layout.count == 0) return;

    const int blockHeight = lineHeight * static_cast<int>(layout.count);
    int y = top + std::max(0, (height - blockHeight) / 2);
    for (std::size_t index = 0; index < layout.count; ++index) {
      drawTextBlock(renderer, text, layout.lines[index], left, width, fontId, style, y);
      y += lineHeight;
    }
    return;
  }

  const MathAssetRenderer mathRenderer(renderer);
  const CardTextBlockLayout layout =
      layoutCardTextBlocks(text, width, height, fontId, maxLines, style, mathRenderer, renderer);
  if (layout.count == 0) return;
  int y = top + std::max(0, (height - layout.totalHeight) / 2);
  for (std::size_t index = 0; index < layout.count; ++index) {
    const CardTextBlock& block = layout.blocks[index];
    if (block.kind == CardTextBlockKind::Text) {
      drawTextBlock(renderer, text, block.line, left, width, fontId, style, y);
      y += block.blockHeight;
      continue;
    }

    const std::string_view formula = text.substr(block.formulaStart, block.formulaLength);
    const int imageX = left + std::max(0, (width - block.width) / 2);
    if (!mathRenderer.draw(formula, imageX, y + MATH_BLOCK_TOP_GAP)) {
      LOG_DBG("StudyMath", "Bitmap draw failed formula=%.*s x=%d y=%d width=%d height=%d",
              static_cast<int>(std::min<std::size_t>(formula.size(), 96)), formula.data(), imageX,
              y + MATH_BLOCK_TOP_GAP, block.width, block.height);
      const CardTextLine fallback{0, formula.size(), 0, false, false};
      drawTextBlock(renderer, formula, fallback, left, width, fontId, style, y + MATH_BLOCK_TOP_GAP);
    }
    y += block.blockHeight;
  }
}

}  // namespace studypet
