#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "CsvDeckParser.h"

namespace {

using studycore::Card;
using studycore::CsvDeckParser;
using studycore::Deck;
using studycore::DeckParseErrorCode;
using studycore::DeckParseResult;
using studycore::parseCsv;

DeckParseResult parseInChunks(const std::string& csv, const std::size_t chunkSize) {
  Deck deck;
  CsvDeckParser parser(deck);
  for (std::size_t offset = 0; offset < csv.size(); offset += chunkSize) {
    const std::size_t size = std::min(chunkSize, csv.size() - offset);
    if (!parser.feed(std::string_view(csv).substr(offset, size))) break;
  }
  parser.finish();

  DeckParseResult result;
  result.deck = std::move(deck);
  result.error = parser.error();
  return result;
}

std::string readFixture(const std::string& filename) {
  std::ifstream file(std::string(STUDY_FIXTURE_DIR) + "/" + filename, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string makeDeck(const std::size_t cardCount) {
  std::string csv = "card_id,front,back\n";
  for (std::size_t i = 0; i < cardCount; ++i) {
    csv += "id-" + std::to_string(i) + ",question,answer\n";
  }
  return csv;
}

void expectError(const std::string& csv, const DeckParseErrorCode code, const std::size_t row) {
  const DeckParseResult result = parseCsv(csv);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error.code, code);
  EXPECT_EQ(result.error.row, row);
  EXPECT_TRUE(result.deck.cards.empty());
}

TEST(StudyDeckParser, ParsesMinimalDeck) {
  const DeckParseResult result =
      parseCsv("card_id,front,back\na,\"2 + 2?\",\"4\"\nb,\"Capital of France?\",\"Paris\"\n");

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 2u);
  EXPECT_EQ(result.deck.cards[0].id, "a");
  EXPECT_EQ(result.deck.cards[0].front, "2 + 2?");
  EXPECT_EQ(result.deck.cards[0].back, "4");
  EXPECT_EQ(result.deck.cards[1].id, "b");
  EXPECT_EQ(result.deck.cards[1].front, "Capital of France?");
  EXPECT_EQ(result.deck.cards[1].back, "Paris");
}

TEST(StudyDeckParser, ParsesQuotedCommasAndEscapedQuotes) {
  const DeckParseResult result = parseCsv(
      "card_id,front,back\n"
      "a,\"Which are valid: A, B, C?\",\"A, B\"\n"
      "b,\"What does \"\"RAII\"\" mean?\",\"A C++ ownership idiom\"\n");

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 2u);
  EXPECT_EQ(result.deck.cards[0].front, "Which are valid: A, B, C?");
  EXPECT_EQ(result.deck.cards[0].back, "A, B");
  EXPECT_EQ(result.deck.cards[1].front, "What does \"RAII\" mean?");
}

TEST(StudyDeckParser, PreservesUtf8Bytes) {
  const DeckParseResult result = parseCsv(
      "card_id,front,back\n"
      "de-1,\"Was bedeutet Übertragung?\",\"transmission\"\n"
      "jp-1,\"日本の首都は？\",\"東京\"\n");

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 2u);
  EXPECT_EQ(result.deck.cards[0].front, "Was bedeutet Übertragung?");
  EXPECT_EQ(result.deck.cards[1].front, "日本の首都は？");
  EXPECT_EQ(result.deck.cards[1].back, "東京");
}

TEST(StudyDeckParser, SupportsCrLfAndBlankLines) {
  const DeckParseResult result = parseCsv("\r\ncard_id,front,back\r\n\r\n a , question , answer \r\n\t\r\nb,q,a\r\n");

  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 2u);
  EXPECT_EQ(result.deck.cards[0].id, " a ");
  EXPECT_EQ(result.deck.cards[0].front, " question ");
  EXPECT_EQ(result.deck.cards[0].back, " answer ");
}

TEST(StudyDeckParser, AcceptsHeaderOnlyAndFinalRecordWithoutNewline) {
  EXPECT_TRUE(parseCsv("card_id,front,back").ok());
  const DeckParseResult result = parseCsv("card_id,front,back\na,q,a");
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 1u);
  EXPECT_EQ(result.deck.cards[0].id, "a");
}

TEST(StudyDeckParser, AcceptsUtf8BomOnHeader) {
  const std::string csv = std::string("\xEF\xBB\xBF") + "card_id,front,back\na,q,a\n";
  EXPECT_TRUE(parseCsv(csv).ok());
}

TEST(StudyDeckParser, UsesStrictHeader) {
  expectError("front,back\nfoo,bar\n", DeckParseErrorCode::InvalidHeader, 1);
  expectError("card_id,back,front\na,b,c\n", DeckParseErrorCode::InvalidHeader, 1);
  expectError("card_id,front,back,extra\na,q,a,x\n", DeckParseErrorCode::InvalidHeader, 1);
  expectError(" card_id,front,back\na,q,a\n", DeckParseErrorCode::InvalidHeader, 1);
}

TEST(StudyDeckParser, RejectsWrongFieldCounts) {
  expectError("card_id,front,back\na,question\n", DeckParseErrorCode::WrongFieldCount, 2);
  expectError("card_id,front,back\na,question,answer,extra\n", DeckParseErrorCode::WrongFieldCount, 2);
}

TEST(StudyDeckParser, RejectsMalformedCsv) {
  expectError("card_id,front,back\na,\"question,answer\n", DeckParseErrorCode::MalformedCsv, 2);
  expectError("card_id,front,back\na,que\"stion,answer\n", DeckParseErrorCode::MalformedCsv, 2);
  expectError("card_id,front,back\na,\"question\"junk,answer\n", DeckParseErrorCode::MalformedCsv, 2);
  expectError("card_id,front,back\na,\"question\n", DeckParseErrorCode::MalformedCsv, 2);
  expectError("card_id,front,back\na,\"question\ranswer\",answer\n", DeckParseErrorCode::MalformedCsv, 2);
}

TEST(StudyDeckParser, RejectsEmptyRequiredFields) {
  expectError("card_id,front,back\n,question,answer\n", DeckParseErrorCode::EmptyCardId, 2);
  expectError("card_id,front,back\nid, ,answer\n", DeckParseErrorCode::EmptyFront, 2);
  expectError("card_id,front,back\nid,question,\t\n", DeckParseErrorCode::EmptyBack, 2);
}

TEST(StudyDeckParser, RejectsDuplicateIdsAndReportsId) {
  const DeckParseResult result = parseCsv("card_id,front,back\nsame,q1,a1\nsame,q2,a2\n");

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error.code, DeckParseErrorCode::DuplicateCardId);
  EXPECT_EQ(result.error.row, 3u);
  EXPECT_EQ(result.error.duplicateId, "same");
  EXPECT_TRUE(result.deck.cards.empty());
}

TEST(StudyDeckParser, EnforcesMaximumDeckSize) {
  const DeckParseResult valid = parseCsv(makeDeck(Deck::MAX_CARDS_PER_DECK));
  ASSERT_TRUE(valid.ok());
  EXPECT_EQ(valid.deck.cards.size(), Deck::MAX_CARDS_PER_DECK);

  const DeckParseResult invalid = parseCsv(makeDeck(Deck::MAX_CARDS_PER_DECK + 1));
  EXPECT_FALSE(invalid.ok());
  EXPECT_EQ(invalid.error.code, DeckParseErrorCode::TooManyCards);
  EXPECT_EQ(invalid.error.row, Deck::MAX_CARDS_PER_DECK + 2);
  EXPECT_TRUE(invalid.deck.cards.empty());
}

TEST(StudyDeckParser, HandlesEmptyAndDefensiveInputs) {
  expectError("", DeckParseErrorCode::InvalidHeader, 1);
  expectError("\n\t\n", DeckParseErrorCode::InvalidHeader, 1);
  expectError("card_id,front,back\r", DeckParseErrorCode::UnexpectedEndOfInput, 1);

  const DeckParseResult longField = parseCsv("card_id,front,back\na,\"" + std::string(4096, 'x') + "\",answer\n");
  ASSERT_TRUE(longField.ok());
  EXPECT_EQ(longField.deck.cards[0].front.size(), 4096u);

  const std::string manyCommas(1000, ',');
  const DeckParseResult commaField = parseCsv("card_id,front,back\na,\"" + manyCommas + "\",answer\n");
  ASSERT_TRUE(commaField.ok());
  EXPECT_EQ(commaField.deck.cards[0].front, manyCommas);
}

TEST(StudyDeckParser, ParsesCanonicalFixture) {
  const std::string fixture = readFixture("sample.csv");
  ASSERT_FALSE(fixture.empty());

  const DeckParseResult result = parseCsv(fixture);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.deck.cards.size(), 6u);
  EXPECT_EQ(result.deck.cards[2].front, "Which are valid: A, B, or C?");
  EXPECT_EQ(result.deck.cards[3].front, "What does \"RAII\" mean?");
  EXPECT_EQ(result.deck.cards[4].front, "Was bedeutet Übertragung?");
  EXPECT_EQ(result.deck.cards[5].back, "東京");
}

TEST(StudyDeckParser, SupportsIncrementalFeedsAcrossEveryByteBoundary) {
  const std::string csv =
      "card_id,front,back\r\n"
      "a,\"A, B\",\"say \"\"hello\"\"\"\r\n"
      "de,\"Übertragung\",\"東京\"\r\n";
  const DeckParseResult expected = parseCsv(csv);
  ASSERT_TRUE(expected.ok());

  for (std::size_t chunkSize = 1; chunkSize <= csv.size(); ++chunkSize) {
    const DeckParseResult actual = parseInChunks(csv, chunkSize);
    ASSERT_TRUE(actual.ok()) << "chunk size " << chunkSize;
    ASSERT_EQ(actual.deck.cards.size(), expected.deck.cards.size()) << "chunk size " << chunkSize;
    for (std::size_t i = 0; i < expected.deck.cards.size(); ++i) {
      EXPECT_EQ(actual.deck.cards[i].id, expected.deck.cards[i].id) << "chunk size " << chunkSize;
      EXPECT_EQ(actual.deck.cards[i].front, expected.deck.cards[i].front) << "chunk size " << chunkSize;
      EXPECT_EQ(actual.deck.cards[i].back, expected.deck.cards[i].back) << "chunk size " << chunkSize;
    }
  }
}

TEST(StudyDeckParser, ClearsDeckAfterIncrementalError) {
  Deck deck;
  CsvDeckParser parser(deck);
  ASSERT_TRUE(parser.feed("card_id,front,back\na,q,a\n"));
  EXPECT_FALSE(parser.feed("a,\"unterminated\n"));
  EXPECT_EQ(parser.error().code, DeckParseErrorCode::MalformedCsv);
  EXPECT_TRUE(deck.cards.empty());
  EXPECT_FALSE(parser.finish());
}

}  // namespace
