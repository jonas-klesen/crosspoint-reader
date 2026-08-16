#pragma once

#include <EpdFontFamily.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class GfxRenderer;

namespace studypet {

/// Four normal spaces per tab stop. Card content keeps the literal tab byte;
/// this value is used only while measuring and drawing.
inline constexpr std::size_t CARD_TEXT_TAB_STOP_SPACES = 4;

/// The review screen never needs more visual lines than this bounded layout.
inline constexpr std::size_t CARD_TEXT_MAX_LINES = 64;

/// A single visual line references the owning Card string. No line owns text.
struct CardTextLine {
  std::size_t start = 0;
  std::size_t length = 0;
  int indentPixels = 0;
  bool leftAligned = false;
  bool ellipsize = false;
};

struct CardTextLayout {
  std::array<CardTextLine, CARD_TEXT_MAX_LINES> lines{};
  std::size_t count = 0;
  bool clipped = false;
};

namespace card_text_detail {

inline constexpr std::size_t MAX_RENDER_LINE_BYTES = 512;

inline bool isUtf8Continuation(const unsigned char value) { return (value & 0xC0U) == 0x80U; }

inline std::size_t nextCodepoint(const std::string_view text, const std::size_t position, const std::size_t end) {
  if (position >= end) return end;
  const unsigned char first = static_cast<unsigned char>(text[position]);
  std::size_t width = 1;
  if ((first & 0x80U) == 0) {
    width = 1;
  } else if ((first & 0xE0U) == 0xC0U) {
    width = 2;
  } else if ((first & 0xF0U) == 0xE0U) {
    width = 3;
  } else if ((first & 0xF8U) == 0xF0U) {
    width = 4;
  }
  if (position + width > end) return position + 1;
  for (std::size_t index = position + 1; index < position + width; ++index) {
    if (!isUtf8Continuation(static_cast<unsigned char>(text[index]))) return position + 1;
  }
  return position + width;
}

inline bool isBreakSpace(const char value) { return value == ' ' || value == '\t'; }

inline bool appendSpaces(std::array<char, MAX_RENDER_LINE_BYTES + 1>& output, std::size_t& length,
                         const std::size_t count) {
  if (count > MAX_RENDER_LINE_BYTES - length) return false;
  for (std::size_t index = 0; index < count; ++index) output[length++] = ' ';
  return true;
}

/// Expand tabs into spaces and copy complete UTF-8 code points only.
inline bool expandRange(const std::string_view text, const std::size_t start, const std::size_t length,
                        std::array<char, MAX_RENDER_LINE_BYTES + 1>& output, std::size_t& outputLength) {
  outputLength = 0;
  const std::size_t end = std::min(text.size(), start + length);
  std::size_t position = std::min(start, text.size());
  std::size_t column = 0;
  while (position < end) {
    if (text[position] == '\t') {
      const std::size_t spaces = CARD_TEXT_TAB_STOP_SPACES - (column % CARD_TEXT_TAB_STOP_SPACES);
      if (!appendSpaces(output, outputLength, spaces)) return false;
      column += spaces;
      ++position;
      continue;
    }

    const std::size_t next = nextCodepoint(text, position, end);
    const std::size_t bytes = next - position;
    if (bytes > MAX_RENDER_LINE_BYTES - outputLength) return false;
    for (std::size_t index = 0; index < bytes; ++index) output[outputLength++] = text[position + index];
    ++column;
    position = next;
  }
  output[outputLength] = '\0';
  return true;
}

inline std::size_t leadingWhitespaceEnd(const std::string_view text, const std::size_t start, const std::size_t end) {
  std::size_t position = start;
  while (position < end && isBreakSpace(text[position])) ++position;
  return position;
}

/// Return the byte after a simple bullet/number prefix, or leading indentation.
inline std::size_t structuredPrefixEnd(const std::string_view text, const std::size_t start, const std::size_t end,
                                       bool& structured) {
  const std::size_t whitespaceEnd = leadingWhitespaceEnd(text, start, end);
  std::size_t prefixEnd = whitespaceEnd;
  structured = whitespaceEnd != start;

  if (whitespaceEnd + 1 < end && (text[whitespaceEnd] == '-' || text[whitespaceEnd] == '*') &&
      isBreakSpace(text[whitespaceEnd + 1])) {
    prefixEnd = whitespaceEnd + 2;
    structured = true;
  } else {
    std::size_t digitsEnd = whitespaceEnd;
    while (digitsEnd < end && text[digitsEnd] >= '0' && text[digitsEnd] <= '9') ++digitsEnd;
    if (digitsEnd > whitespaceEnd && digitsEnd < end && text[digitsEnd] == '.') {
      prefixEnd = digitsEnd + 1;
      if (prefixEnd < end && isBreakSpace(text[prefixEnd])) ++prefixEnd;
      structured = true;
    }
  }
  return prefixEnd;
}

}  // namespace card_text_detail

/// Layout structured plain text with a caller-supplied UTF-8-aware width function.
/// The function receives a NUL-terminated, tab-expanded visual line.
template <typename MeasureFn>
CardTextLayout layoutCardText(const std::string_view text, const int maxWidth, const int maxLines,
                              MeasureFn&& measure) {
  CardTextLayout layout;
  if (text.empty() || maxWidth <= 0 || maxLines <= 0) return layout;

  const std::size_t lineLimit = std::min<std::size_t>(CARD_TEXT_MAX_LINES, static_cast<std::size_t>(maxLines));
  auto measureRange = [&](const std::size_t start, const std::size_t length) {
    std::array<char, card_text_detail::MAX_RENDER_LINE_BYTES + 1> buffer{};
    std::size_t outputLength = 0;
    if (!card_text_detail::expandRange(text, start, length, buffer, outputLength)) return maxWidth + 1;
    return std::max(0, measure(buffer.data()));
  };

  auto appendLine = [&](const std::size_t start, const std::size_t length, const int indentPixels,
                        const bool leftAligned) {
    if (layout.count >= lineLimit) {
      layout.clipped = true;
      if (layout.count > 0) layout.lines[layout.count - 1].ellipsize = true;
      return false;
    }
    layout.lines[layout.count++] = CardTextLine{start, length, indentPixels, leftAligned, false};
    return true;
  };

  auto skipBreakSpaces = [&](std::size_t position, const std::size_t end) {
    while (position < end && card_text_detail::isBreakSpace(text[position])) ++position;
    return position;
  };

  std::size_t logicalStart = 0;
  while (logicalStart <= text.size()) {
    const std::size_t newline = text.find('\n', logicalStart);
    const std::size_t logicalEnd = newline == std::string_view::npos ? text.size() : newline;
    if (logicalStart == logicalEnd) {
      if (!appendLine(logicalStart, 0, 0, false)) return layout;
    } else {
      bool structured = false;
      const std::size_t prefixEnd = card_text_detail::structuredPrefixEnd(text, logicalStart, logicalEnd, structured);
      const int measuredIndent = prefixEnd > logicalStart ? measureRange(logicalStart, prefixEnd - logicalStart) : 0;
      const int continuationIndent = std::clamp(measuredIndent, 0, std::max(0, maxWidth - 1));
      std::size_t segmentStart = logicalStart;
      bool firstSegment = true;

      while (segmentStart < logicalEnd) {
        const int indent = firstSegment ? 0 : continuationIndent;
        const int availableWidth = std::max(1, maxWidth - indent);
        std::size_t cursor = segmentStart;
        std::size_t fitEnd = segmentStart;
        std::size_t lastBreak = std::string_view::npos;
        bool overflow = false;

        while (cursor < logicalEnd) {
          const std::size_t next = card_text_detail::nextCodepoint(text, cursor, logicalEnd);
          const int candidateWidth = measureRange(segmentStart, next - segmentStart);
          if (candidateWidth > availableWidth) {
            overflow = true;
            break;
          }
          fitEnd = next;
          cursor = next;
          if (cursor > segmentStart && card_text_detail::isBreakSpace(text[cursor - 1]) &&
              !(firstSegment && cursor <= prefixEnd)) {
            lastBreak = cursor;
          }
        }

        if (!overflow) {
          if (!appendLine(segmentStart, logicalEnd - segmentStart, indent, structured)) return layout;
          break;
        }

        std::size_t lineEnd = fitEnd;
        std::size_t nextStart = fitEnd;
        if (lastBreak != std::string_view::npos && lastBreak > segmentStart) {
          lineEnd = lastBreak;
          while (lineEnd > segmentStart && card_text_detail::isBreakSpace(text[lineEnd - 1])) --lineEnd;
          nextStart = skipBreakSpaces(lastBreak, logicalEnd);
        }
        if (lineEnd == segmentStart) {
          const std::size_t next = card_text_detail::nextCodepoint(text, segmentStart, logicalEnd);
          lineEnd = next;
          nextStart = next;
        }

        if (!appendLine(segmentStart, lineEnd - segmentStart, indent, structured)) return layout;
        if (nextStart >= logicalEnd) break;
        segmentStart = nextStart;
        firstSegment = false;
      }
    }

    if (newline == std::string_view::npos) break;
    logicalStart = logicalEnd + 1;
    if (layout.count >= lineLimit && logicalStart <= text.size()) {
      layout.clipped = true;
      layout.lines[layout.count - 1].ellipsize = true;
      return layout;
    }
  }

  return layout;
}

class CardTextRenderer final {
 public:
  explicit CardTextRenderer(GfxRenderer& renderer) : renderer(renderer) {}

  void draw(std::string_view text, int left, int top, int width, int height, int fontId, int maxLines,
            EpdFontFamily::Style style) const;

 private:
  GfxRenderer& renderer;
};

}  // namespace studypet
