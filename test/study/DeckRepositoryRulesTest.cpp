#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "DeckRepository.h"
#include "DeckRepositoryRules.h"

namespace {

using studypet::deckDisplayName;
using studypet::deckDisplayNameForRelativePath;
using studypet::deckEntryLess;
using studypet::hasCsvExtension;
using studypet::isDeckCandidate;
using studypet::isSafeDeckFilename;
using studypet::isSafeRelativeDeckPath;
using studypet::isSafeRelativeDirectory;
using studypet::joinRelativeDeckPath;

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

TEST(DeckRepositoryRules, RejectsControlCharacters) {
  EXPECT_FALSE(isSafeDeckFilename(std::string_view("deck\n.csv")));
  EXPECT_FALSE(isSafeDeckFilename(std::string_view("deck\0.csv", 9)));
  EXPECT_FALSE(isSafeDeckFilename(std::string_view("deck\x7f.csv", 9)));
  EXPECT_FALSE(isDeckCandidate(std::string_view("deck\t.csv")));
}

TEST(DeckRepositoryRules, RequiresCsvSuffixForCandidates) {
  EXPECT_FALSE(isDeckCandidate("notes.csv.txt"));
  EXPECT_FALSE(isDeckCandidate("deck.csv.bak"));
  EXPECT_TRUE(isDeckCandidate("deck.CsV"));
  EXPECT_FALSE(isDeckCandidate("deck"));
  EXPECT_FALSE(isDeckCandidate("deck."));
}

TEST(DeckRepositoryRules, KeepsDisplayNameOfNonCsvUntouched) {
  EXPECT_EQ(deckDisplayName("notes.txt"), "notes.txt");
  EXPECT_EQ(deckDisplayName(""), "");
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

TEST(DeckRepositoryRules, AcceptsCanonicalRelativeDirectoriesAndDecks) {
  EXPECT_TRUE(isSafeRelativeDirectory(""));
  EXPECT_TRUE(isSafeRelativeDirectory("University"));
  EXPECT_TRUE(isSafeRelativeDirectory("University/Networks"));
  EXPECT_TRUE(isSafeRelativeDeckPath("University/Networks/tcp.csv"));
}

TEST(DeckRepositoryRules, RejectsRelativePathEscapesAndMalformedSeparators) {
  EXPECT_FALSE(isSafeRelativeDeckPath("../foo.csv"));
  EXPECT_FALSE(isSafeRelativeDeckPath("University/../foo.csv"));
  EXPECT_FALSE(isSafeRelativeDeckPath("/absolute.csv"));
  EXPECT_FALSE(isSafeRelativeDeckPath("University//foo.csv"));
  EXPECT_FALSE(isSafeRelativeDeckPath(""));
  EXPECT_FALSE(isSafeRelativeDirectory(".crosspoint"));
  EXPECT_FALSE(isSafeRelativeDirectory("University/"));
}
TEST(DeckRepositoryRules, EnforcesNamedComponentAndLocationBounds) {
  const std::string overlongComponent(studypet::MAX_DECK_COMPONENT_BYTES + 1, 'x');
  EXPECT_FALSE(isSafeDeckFilename(overlongComponent));

  std::string overlongPath;
  for (int component = 0; component < 9; ++component) {
    if (!overlongPath.empty()) overlongPath.push_back('/');
    overlongPath.append(studypet::MAX_DECK_COMPONENT_BYTES, 'x');
  }
  EXPECT_GT(overlongPath.size(), studypet::MAX_DECK_LOCATION_BYTES);
  EXPECT_FALSE(isSafeRelativeDirectory(overlongPath));
}

TEST(DeckRepositoryRules, JoinsOnlySafeRelativeComponents) {
  std::string joined;
  EXPECT_TRUE(joinRelativeDeckPath("", "networks.csv", joined));
  EXPECT_EQ(joined, "networks.csv");
  EXPECT_TRUE(joinRelativeDeckPath("University", "Networks", joined));
  EXPECT_EQ(joined, "University/Networks");
  EXPECT_FALSE(joinRelativeDeckPath("University/../", "foo.csv", joined));
  EXPECT_FALSE(joinRelativeDeckPath("University", "../foo.csv", joined));
}

TEST(DeckRepositoryRules, DerivesNestedDeckDisplayNames) {
  EXPECT_EQ(deckDisplayNameForRelativePath("University/Networks/tcp.csv"), "tcp");
  EXPECT_EQ(deckDisplayNameForRelativePath("misc.csv"), "misc");
}

TEST(DeckRepositoryRules, NaturalSortGroupsFoldersBeforeDecks) {
  struct Entry {
    bool folder;
    std::string name;
  };
  std::vector<Entry> entries{
      {false, "deck10"}, {true, "University"}, {false, "deck2"}, {true, "Languages"}, {false, "deck1"}};

  std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
    return deckEntryLess(left.folder, left.name, right.folder, right.name);
  });

  ASSERT_EQ(entries.size(), 5U);
  EXPECT_EQ(entries[0].name, "Languages");
  EXPECT_EQ(entries[1].name, "University");
  EXPECT_EQ(entries[2].name, "deck1");
  EXPECT_EQ(entries[3].name, "deck2");
  EXPECT_EQ(entries[4].name, "deck10");
}

TEST(DeckRepositoryRules, DeckLocationsKeepDuplicateBasenamesDistinct) {
  studypet::DeckLocation university;
  studypet::DeckLocation personal;
  ASSERT_TRUE(studypet::DeckLocation::tryCreateDeck("University/networks.csv", university));
  ASSERT_TRUE(studypet::DeckLocation::tryCreateDeck("Personal/networks.csv", personal));
  EXPECT_NE(university.relativePath(), personal.relativePath());
  EXPECT_EQ(university.relativePath(), "University/networks.csv");
  EXPECT_EQ(personal.relativePath(), "Personal/networks.csv");
}

TEST(DeckRepositoryRules, DeckLocationRejectsAbsoluteAndTraversalPaths) {
  studypet::DeckLocation location;
  EXPECT_FALSE(studypet::DeckLocation::tryCreateDeck("../foo.csv", location));
  EXPECT_FALSE(studypet::DeckLocation::tryCreateDeck("/etc/foo.csv", location));
  EXPECT_FALSE(studypet::DeckLocation::tryCreateDeck("University//foo.csv", location));
}

}  // namespace
