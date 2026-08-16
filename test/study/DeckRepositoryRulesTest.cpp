#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "DeckRepositoryRules.h"

namespace {

using studypet::deckDisplayName;
using studypet::hasCsvExtension;
using studypet::isDeckCandidate;
using studypet::isSafeDeckFilename;

TEST(DeckRepositoryRules, AcceptsCsvNamesCaseInsensitively) {
  EXPECT_TRUE(hasCsvExtension("basic.csv"));
  EXPECT_TRUE(hasCsvExtension("BASIC.CSV"));
  EXPECT_TRUE(isDeckCandidate("my deck.csv"));
  EXPECT_TRUE(isDeckCandidate("netzwerke-übung.csv"));
}

TEST(DeckRepositoryRules, IgnoresNonDeckAndTemporaryNames) {
  EXPECT_FALSE(isDeckCandidate("notes.txt"));
  EXPECT_FALSE(isDeckCandidate("deck.csv.bak"));
  EXPECT_FALSE(isDeckCandidate(".hidden.csv"));
  EXPECT_FALSE(isDeckCandidate("deck.csv~"));
  EXPECT_FALSE(isDeckCandidate("deck.csv.tmp"));
  EXPECT_FALSE(isDeckCandidate("deck.csv.part"));
}

TEST(DeckRepositoryRules, RejectsPathLikeNames) {
  EXPECT_FALSE(isSafeDeckFilename("../deck.csv"));
  EXPECT_FALSE(isSafeDeckFilename("nested/deck.csv"));
  EXPECT_FALSE(isSafeDeckFilename("nested\\deck.csv"));
  EXPECT_FALSE(isSafeDeckFilename(""));
  EXPECT_FALSE(isSafeDeckFilename(".."));
}

TEST(DeckRepositoryRules, DerivesDisplayNameWithoutGuessing) {
  EXPECT_EQ(deckDisplayName("networks.csv"), "networks");
  EXPECT_EQ(deckDisplayName("CS_101-final.CSV"), "CS_101-final");
  EXPECT_EQ(deckDisplayName("my deck.csv"), "my deck");
  EXPECT_EQ(deckDisplayName("notes.txt"), "notes.txt");
}

TEST(DeckRepositoryRules, PreservesUtf8Bytes) {
  constexpr std::string_view filename = "netzwerke-übung.csv";
  EXPECT_EQ(deckDisplayName(filename), "netzwerke-übung");
}

}  // namespace
