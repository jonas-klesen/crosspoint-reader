#pragma once

#include "Card.h"

#include <cstddef>
#include <vector>

namespace studycore {

struct Deck {
  static constexpr std::size_t MAX_CARDS_PER_DECK = 500;

  std::vector<Card> cards;
};

}
