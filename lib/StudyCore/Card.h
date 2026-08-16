#pragma once

#include <cstddef>
#include <string>

namespace studycore {

struct Card {
  /// Maximum encoded bytes for a card ID field.
  static constexpr std::size_t MAX_ID_BYTES = 128;

  /// Maximum encoded bytes for a card front or back text field.
  static constexpr std::size_t MAX_TEXT_BYTES = 4096;

  std::string id;
  std::string front;
  std::string back;
};

}  // namespace studycore
