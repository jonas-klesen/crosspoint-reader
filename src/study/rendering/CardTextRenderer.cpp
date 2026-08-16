#include "CardTextRenderer.h"
#if defined(STUDYPET_MATH_SPIKE)
#include "experimental/MathCardTextRenderer.h"
#endif

#include <GfxRenderer.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace studypet {

void CardTextRenderer::draw(const std::string_view text, const int left, const int top, const int width,
                            const int height, const int fontId, const int maxLines,
                            const EpdFontFamily::Style style) const {
  if (text.empty() || width <= 0 || height <= 0 || maxLines <= 0) return;
#if defined(STUDYPET_MATH_SPIKE)
  if (mathspike::MathCardTextRenderer::containsMath(text)) {
    mathspike::MathCardTextRenderer(renderer).draw(text, left, top, width, height, fontId, maxLines, style);
    return;
  }
#endif

  const int lineHeight = renderer.getLineHeight(fontId);
  if (lineHeight <= 0) return;

  const auto measure = [&](const char* line) { return renderer.getTextWidth(fontId, line, style); };
  const CardTextLayout layout = layoutCardText(text, width, maxLines, measure);
  if (layout.count == 0) return;

  const int blockHeight = lineHeight * static_cast<int>(layout.count);
  int y = top + std::max(0, (height - blockHeight) / 2);
  for (std::size_t index = 0; index < layout.count; ++index) {
    const CardTextLine& line = layout.lines[index];
    std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
    std::size_t outputLength = 0;
    if (!card_text_detail::expandRange(text, line.start, line.length, buffer, outputLength)) {
      y += lineHeight;
      continue;
    }

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
    y += lineHeight;
  }
}

}  // namespace studypet
