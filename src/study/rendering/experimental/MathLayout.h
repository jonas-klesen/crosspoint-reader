#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace studypet::mathspike {

enum class MathSymbol : uint8_t {
  Literal,
  Space,
  Alpha,
  Beta,
  Gamma,
  Delta,
  Epsilon,
  Lambda,
  Mu,
  Pi,
  Sigma,
  Theta,
  Sum,
  Product,
  Integral,
  Infinity,
  LessEqual,
  GreaterEqual,
  NotEqual,
  Approx,
  Arrow,
};

struct MathMetrics {
  int bodyAscent = 16;
  int bodyDescent = 5;
  int scriptAscent = 9;
  int scriptDescent = 4;
  int scriptGap = 2;
  int atomGap = 1;
  int fractionGap = 2;
  int fractionRule = 1;
  int fractionPadding = 3;
  int radicalWidth = 7;
};

inline constexpr std::size_t MAX_ROW_CHILDREN = 32;

struct MathNode {
  enum class Kind : uint8_t { Row, Glyph, Fraction, Sqrt, Script };

  static constexpr uint8_t NONE = 0xFF;

  Kind kind = Kind::Glyph;
  MathSymbol symbol = MathSymbol::Literal;
  char literal = '\0';
  uint8_t first = NONE;
  uint8_t second = NONE;
  uint8_t third = NONE;
  uint8_t childCount = 0;
  std::array<uint8_t, MAX_ROW_CHILDREN> children{};
  int16_t width = 0;
  int16_t ascent = 0;
  int16_t descent = 0;
};

class MathParser;

class MathLayout {
 public:
  static constexpr std::size_t MAX_SOURCE_BYTES = 512;
  static constexpr std::size_t MAX_NODES = 96;
  static constexpr uint8_t MAX_NESTING = 8;

  MathLayout() = default;

  static MathLayout parse(std::string_view source) noexcept;

  bool valid() const { return valid_; }
  bool clipped() const { return clipped_; }
  uint8_t root() const { return root_; }
  std::size_t nodeCount() const { return nodeCount_; }
  const MathNode& node(uint8_t index) const { return nodes_[index]; }
  uint8_t child(uint8_t row, uint8_t offset) const { return nodes_[row].children[offset]; }
  int width() const { return valid_ ? nodes_[root_].width : 0; }
  int ascent() const { return valid_ ? nodes_[root_].ascent : 0; }
  int descent() const { return valid_ ? nodes_[root_].descent : 0; }
  int height() const { return ascent() + descent(); }

 private:
  friend class MathParser;

  uint8_t addNode(MathNode node) noexcept;
  bool addChild(uint8_t row, uint8_t child) noexcept;
  void measureNode(uint8_t index, const MathMetrics& metrics, uint8_t depth) noexcept;
  static int symbolWidth(MathSymbol symbol, bool script, const MathMetrics& metrics) noexcept;

  std::array<MathNode, MAX_NODES> nodes_{};
  std::size_t nodeCount_ = 0;
  uint8_t root_ = MathNode::NONE;
  bool valid_ = false;
  bool clipped_ = false;
};

class MathParser {
 public:
  explicit MathParser(std::string_view source) : source_(source) {}

  MathLayout parse() noexcept {
    MathLayout result;
    if (source_.empty() || source_.size() > MathLayout::MAX_SOURCE_BYTES) {
      result.clipped_ = source_.size() > MathLayout::MAX_SOURCE_BYTES;
      return result;
    }

    layout_ = &result;
    const uint8_t root = parseRow('\0', 0);
    result.root_ = root;
    if (root == MathNode::NONE || failed_ || position_ != source_.size()) return result;

    result.measureNode(root, MathMetrics{}, 0);
    result.valid_ = !result.clipped_;
    return result;
  }

 private:
  uint8_t parseRow(char terminator, uint8_t depth) noexcept {
    if (depth > MathLayout::MAX_NESTING) {
      fail();
      return MathNode::NONE;
    }

    const uint8_t row = layout_->addNode(MathNode{MathNode::Kind::Row});
    if (row == MathNode::NONE) return MathNode::NONE;

    while (position_ < source_.size()) {
      const char current = source_[position_];
      if (current == terminator) {
        ++position_;
        if (layout_->node(row).childCount == 0) fail();
        return failed_ ? MathNode::NONE : row;
      }
      if (current == '}') {
        fail();
        return MathNode::NONE;
      }
      if (current == ' ' || current == '\t' || current == '\n' || current == '\r') {
        ++position_;
        continue;
      }

      const uint8_t atom = parseAtom(depth);
      if (atom == MathNode::NONE) return MathNode::NONE;
      const uint8_t scripted = parseScripts(atom, depth);
      if (scripted == MathNode::NONE || !layout_->addChild(row, scripted)) return MathNode::NONE;
    }

    if (terminator != '\0' || layout_->node(row).childCount == 0) fail();
    return failed_ ? MathNode::NONE : row;
  }

  uint8_t parseAtom(uint8_t depth) noexcept {
    if (position_ >= source_.size()) {
      fail();
      return MathNode::NONE;
    }

    const char current = source_[position_++];
    if (current == '{') return parseRow('}', static_cast<uint8_t>(depth + 1));
    if (static_cast<unsigned char>(current) >= 0x80U || current == '}' || current == '^' || current == '_') {
      fail();
      return MathNode::NONE;
    }
    if (current == '\\') return parseCommand(depth);

    MathNode glyph;
    glyph.kind = MathNode::Kind::Glyph;
    glyph.symbol = MathSymbol::Literal;
    glyph.literal = current;
    return layout_->addNode(glyph);
  }

  uint8_t parseCommand(uint8_t depth) noexcept {
    if (position_ >= source_.size()) {
      fail();
      return MathNode::NONE;
    }
    if (source_[position_] == ' ') {
      ++position_;
      return makeSymbol(MathSymbol::Space);
    }
    if (source_[position_] == ',') {
      ++position_;
      return makeSymbol(MathSymbol::Space);
    }

    const std::size_t start = position_;
    while (position_ < source_.size() && isCommandLetter(source_[position_])) ++position_;
    if (start == position_) {
      fail();
      return MathNode::NONE;
    }
    const std::string_view command = source_.substr(start, position_ - start);

    if (command == "frac") {
      const uint8_t numerator = parseArgument(depth);
      const uint8_t denominator = parseArgument(depth);
      if (numerator == MathNode::NONE || denominator == MathNode::NONE) return MathNode::NONE;
      MathNode fraction;
      fraction.kind = MathNode::Kind::Fraction;
      fraction.first = numerator;
      fraction.second = denominator;
      return layout_->addNode(fraction);
    }
    if (command == "sqrt") {
      if (position_ < source_.size() && source_[position_] == '[') {
        fail();
        return MathNode::NONE;
      }
      const uint8_t radicand = parseArgument(depth);
      if (radicand == MathNode::NONE) return MathNode::NONE;
      MathNode root;
      root.kind = MathNode::Kind::Sqrt;
      root.first = radicand;
      return layout_->addNode(root);
    }

    const MathSymbol symbol = symbolFor(command);
    if (symbol == MathSymbol::Literal) {
      fail();
      return MathNode::NONE;
    }
    return makeSymbol(symbol);
  }

  uint8_t parseArgument(uint8_t depth) noexcept {
    if (position_ >= source_.size()) {
      fail();
      return MathNode::NONE;
    }
    if (source_[position_] == '{') {
      ++position_;
      return parseRow('}', static_cast<uint8_t>(depth + 1));
    }
    return parseAtom(static_cast<uint8_t>(depth + 1));
  }

  uint8_t parseScripts(uint8_t base, uint8_t depth) noexcept {
    uint8_t superscript = MathNode::NONE;
    uint8_t subscript = MathNode::NONE;
    while (position_ < source_.size() && (source_[position_] == '^' || source_[position_] == '_')) {
      const bool isSuperscript = source_[position_] == '^';
      ++position_;
      const uint8_t script = parseArgument(static_cast<uint8_t>(depth + 1));
      if (script == MathNode::NONE) return MathNode::NONE;
      uint8_t& target = isSuperscript ? superscript : subscript;
      if (target != MathNode::NONE) {
        fail();
        return MathNode::NONE;
      }
      target = script;
    }
    if (superscript == MathNode::NONE && subscript == MathNode::NONE) return base;

    MathNode scripts;
    scripts.kind = MathNode::Kind::Script;
    scripts.first = base;
    scripts.second = superscript;
    scripts.third = subscript;
    return layout_->addNode(scripts);
  }

  uint8_t makeSymbol(MathSymbol symbol) noexcept {
    MathNode glyph;
    glyph.kind = MathNode::Kind::Glyph;
    glyph.symbol = symbol;
    return layout_->addNode(glyph);
  }

  static bool isCommandLetter(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
  }

  static MathSymbol symbolFor(std::string_view command) noexcept {
    if (command == "alpha") return MathSymbol::Alpha;
    if (command == "beta") return MathSymbol::Beta;
    if (command == "gamma") return MathSymbol::Gamma;
    if (command == "delta") return MathSymbol::Delta;
    if (command == "epsilon") return MathSymbol::Epsilon;
    if (command == "lambda") return MathSymbol::Lambda;
    if (command == "mu") return MathSymbol::Mu;
    if (command == "pi") return MathSymbol::Pi;
    if (command == "sigma") return MathSymbol::Sigma;
    if (command == "theta") return MathSymbol::Theta;
    if (command == "sum") return MathSymbol::Sum;
    if (command == "prod") return MathSymbol::Product;
    if (command == "int") return MathSymbol::Integral;
    if (command == "infty") return MathSymbol::Infinity;
    if (command == "le") return MathSymbol::LessEqual;
    if (command == "ge") return MathSymbol::GreaterEqual;
    if (command == "neq") return MathSymbol::NotEqual;
    if (command == "approx") return MathSymbol::Approx;
    if (command == "to") return MathSymbol::Arrow;
    return MathSymbol::Literal;
  }

  void fail() noexcept { failed_ = true; }

  std::string_view source_;
  MathLayout* layout_ = nullptr;
  std::size_t position_ = 0;
  bool failed_ = false;
};

inline MathLayout MathLayout::parse(std::string_view source) noexcept { return MathParser(source).parse(); }

inline uint8_t MathLayout::addNode(MathNode node) noexcept {
  if (nodeCount_ >= MAX_NODES) {
    clipped_ = true;
    return MathNode::NONE;
  }
  nodes_[nodeCount_] = node;
  return static_cast<uint8_t>(nodeCount_++);
}

inline bool MathLayout::addChild(uint8_t row, uint8_t child) noexcept {
  if (row == MathNode::NONE || child == MathNode::NONE || row >= nodeCount_) {
    clipped_ = true;
    return false;
  }
  MathNode& parent = nodes_[row];
  if (parent.kind != MathNode::Kind::Row || parent.childCount >= MAX_ROW_CHILDREN) {
    clipped_ = true;
    return false;
  }
  parent.children[parent.childCount++] = child;
  return true;
}

inline int MathLayout::symbolWidth(MathSymbol symbol, bool script, const MathMetrics&) noexcept {
  if (symbol == MathSymbol::Space) return script ? 3 : 5;
  if (symbol == MathSymbol::Literal) return script ? 8 : 13;
  switch (symbol) {
    case MathSymbol::LessEqual:
    case MathSymbol::GreaterEqual:
    case MathSymbol::NotEqual:
    case MathSymbol::Approx:
    case MathSymbol::Arrow:
      return script ? 9 : 15;
    case MathSymbol::Sum:
    case MathSymbol::Product:
      return script ? 10 : 16;
    case MathSymbol::Integral:
      return script ? 8 : 12;
    case MathSymbol::Infinity:
      return script ? 9 : 14;
    default:
      return script ? 11 : 12;
  }
}

inline void MathLayout::measureNode(uint8_t index, const MathMetrics& metrics, uint8_t depth) noexcept {
  if (index == MathNode::NONE || index >= nodeCount_ || depth > MAX_NESTING + 2) {
    clipped_ = true;
    return;
  }
  MathNode& current = nodes_[index];
  switch (current.kind) {
    case MathNode::Kind::Glyph:
      current.width = static_cast<int16_t>(symbolWidth(current.symbol, false, metrics));
      current.ascent = static_cast<int16_t>(current.symbol == MathSymbol::Space ? 0 : metrics.bodyAscent);
      current.descent = static_cast<int16_t>(current.symbol == MathSymbol::Space ? 0 : metrics.bodyDescent);
      return;
    case MathNode::Kind::Row: {
      int width = 0;
      int ascent = 0;
      int descent = 0;
      for (uint8_t offset = 0; offset < current.childCount; ++offset) {
        const uint8_t child = current.children[offset];
        measureNode(child, metrics, static_cast<uint8_t>(depth + 1));
        const MathNode& measured = nodes_[child];
        if (offset != 0) width += metrics.atomGap;
        width += measured.width;
        ascent = std::max(ascent, static_cast<int>(measured.ascent));
        descent = std::max(descent, static_cast<int>(measured.descent));
      }
      current.width = static_cast<int16_t>(std::min(width, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.ascent = static_cast<int16_t>(std::min(ascent, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.descent = static_cast<int16_t>(std::min(descent, static_cast<int>(std::numeric_limits<int16_t>::max())));
      return;
    }
    case MathNode::Kind::Fraction: {
      measureNode(current.first, metrics, static_cast<uint8_t>(depth + 1));
      measureNode(current.second, metrics, static_cast<uint8_t>(depth + 1));
      const MathNode& numerator = nodes_[current.first];
      const MathNode& denominator = nodes_[current.second];
      const int numeratorHeight = numerator.ascent + numerator.descent;
      const int denominatorHeight = denominator.ascent + denominator.descent;
      const int total =
          numeratorHeight + metrics.fractionGap + metrics.fractionRule + metrics.fractionGap + denominatorHeight;
      const int width = std::max(static_cast<int>(numerator.width), static_cast<int>(denominator.width)) +
                        2 * metrics.fractionPadding;
      current.width = static_cast<int16_t>(std::min(width, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.ascent =
          static_cast<int16_t>(std::min((total + 1) / 2, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.descent =
          static_cast<int16_t>(std::min(total - current.ascent, static_cast<int>(std::numeric_limits<int16_t>::max())));
      return;
    }
    case MathNode::Kind::Sqrt: {
      measureNode(current.first, metrics, static_cast<uint8_t>(depth + 1));
      const MathNode& radicand = nodes_[current.first];
      const int width = static_cast<int>(radicand.width) + metrics.radicalWidth;
      current.width = static_cast<int16_t>(std::min(width, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.ascent = static_cast<int16_t>(
          std::min(static_cast<int>(radicand.ascent) + 2, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.descent = radicand.descent;
      return;
    }
    case MathNode::Kind::Script: {
      measureNode(current.first, metrics, static_cast<uint8_t>(depth + 1));
      const MathNode& base = nodes_[current.first];
      int scriptWidth = 0;
      int scriptAscent = 0;
      int scriptDescent = 0;
      if (current.second != MathNode::NONE) {
        measureNode(current.second, metrics, static_cast<uint8_t>(depth + 1));
        const MathNode& superscript = nodes_[current.second];
        scriptWidth = std::max(scriptWidth, static_cast<int>(superscript.width));
        scriptAscent = superscript.ascent + superscript.descent + metrics.scriptGap;
      }
      if (current.third != MathNode::NONE) {
        measureNode(current.third, metrics, static_cast<uint8_t>(depth + 1));
        const MathNode& subscript = nodes_[current.third];
        scriptWidth = std::max(scriptWidth, static_cast<int>(subscript.width));
        scriptDescent = subscript.ascent + subscript.descent + metrics.scriptGap;
      }
      const int width = static_cast<int>(base.width) + scriptWidth;
      current.width = static_cast<int16_t>(std::min(width, static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.ascent = static_cast<int16_t>(std::min(static_cast<int>(base.ascent) + scriptAscent,
                                                     static_cast<int>(std::numeric_limits<int16_t>::max())));
      current.descent = static_cast<int16_t>(std::min(std::max(static_cast<int>(base.descent), scriptDescent),
                                                      static_cast<int>(std::numeric_limits<int16_t>::max())));
      return;
    }
  }
}

inline MathLayout parseMath(std::string_view source) noexcept { return MathLayout::parse(source); }

}  // namespace studypet::mathspike
