#pragma once

#include <EpdFontFamily.h>

#include <string_view>

class GfxRenderer;

namespace studypet::mathspike {

class MathCardTextRenderer final {
 public:
  explicit MathCardTextRenderer(GfxRenderer& renderer) : renderer_(renderer) {}

  static bool containsMath(std::string_view text) noexcept;

  void draw(std::string_view text, int left, int top, int width, int height, int fontId, int maxLines,
            EpdFontFamily::Style style) const;

 private:
  GfxRenderer& renderer_;
};

}  // namespace studypet::mathspike
