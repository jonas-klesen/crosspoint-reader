#pragma once

#include <cstdint>
#include <string_view>

class GfxRenderer;

namespace studypet {

enum class MathAssetStatus : uint8_t {
  Valid,
  Missing,
  InvalidPath,
  ReadFailed,
  InvalidMagic,
  ZeroWidth,
  ZeroHeight,
  WidthTooLarge,
  HeightTooLarge,
  SizeMismatch,
  OutOfBounds,
};

struct MathAssetInfo {
  MathAssetStatus status = MathAssetStatus::Missing;
  uint16_t width = 0;
  uint16_t height = 0;

  bool valid() const { return status == MathAssetStatus::Valid; }
};

class MathAssetRenderer final {
 public:
  explicit MathAssetRenderer(GfxRenderer& renderer) : renderer_(renderer) {}

  MathAssetInfo inspect(std::string_view formula) const;
  bool draw(std::string_view formula, int x, int y) const;

 private:
  GfxRenderer& renderer_;
};

}  // namespace studypet
