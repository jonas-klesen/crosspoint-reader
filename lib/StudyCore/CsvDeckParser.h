#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "Deck.h"

namespace studycore {

enum class DeckParseErrorCode {
  None,
  InvalidHeader,
  WrongFieldCount,
  MalformedCsv,
  EmptyCardId,
  EmptyFront,
  EmptyBack,
  DuplicateCardId,
  TooManyCards,
  FieldTooLong,
  UnexpectedEndOfInput,
};

struct DeckParseError {
  DeckParseErrorCode code = DeckParseErrorCode::None;
  std::size_t row = 0;
  std::string duplicateId;
};

// Streams CSV deck data into a caller-owned Deck. Quoted fields may contain LF
// or CRLF; embedded CRLF is stored as one LF. Record separators remain outside
// quoted fields, and all pending separator/newline state survives feed() chunk
// boundaries. The referenced Deck must outlive the parser: feed()/finish() write
// into it directly, so a parser that outlives its Deck would hold a dangling
// reference. Copies and moves are deleted because moving a parser would leave
// the source holding a reference to a destination that no longer reflects the
// source's streaming state.
class CsvDeckParser {
 public:
  explicit CsvDeckParser(Deck& deck);

  CsvDeckParser(const CsvDeckParser&) = delete;
  CsvDeckParser& operator=(const CsvDeckParser&) = delete;
  CsvDeckParser(CsvDeckParser&&) = delete;
  CsvDeckParser& operator=(CsvDeckParser&&) = delete;

  bool feed(std::string_view bytes);
  bool finish();

  bool ok() const { return parseError.code == DeckParseErrorCode::None; }
  const DeckParseError& error() const { return parseError; }

 private:
  enum class State {
    FieldStart,
    InUnquotedField,
    InQuotedField,
    AfterQuotedQuote,
  };

  static bool isAsciiWhitespace(char value);
  static bool isBlankField(const std::string& value);
  static std::size_t fieldLimitFor(std::size_t fieldIndex);

  bool canAppendFieldByte() const;
  bool appendFieldByte(char value);
  bool appendField();
  bool finishRecord();

  bool finishInputRecord();
  bool hasCurrentRecord() const;
  bool hasPendingFinalField() const;
  bool isBlankRecord() const;
  bool processHeader();
  bool processCard();
  bool setError(DeckParseErrorCode code, std::size_t errorRow);
  bool setDuplicateError(const std::string& id);
  void resetRecord();

  Deck& deck;
  DeckParseError parseError;
  std::size_t row = 1;
  State state = State::FieldStart;
  std::string field;
  std::string fields[4];
  std::size_t fieldCount = 0;
  bool rowHadComma = false;
  bool rowHadQuote = false;
  bool pendingCarriageReturn = false;
  bool pendingQuotedCarriageReturn = false;
  bool headerSeen = false;
  bool finished = false;
};

struct DeckParseResult {
  Deck deck;
  DeckParseError error;

  bool ok() const { return error.code == DeckParseErrorCode::None; }
};

DeckParseResult parseCsv(std::string_view csv);

}  // namespace studycore
