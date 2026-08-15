#pragma once

#include "Deck.h"

#include <cstddef>
#include <string>
#include <string_view>

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
  UnexpectedEndOfInput,
};

struct DeckParseError {
  DeckParseErrorCode code = DeckParseErrorCode::None;
  std::size_t row = 0;
  std::string duplicateId;
};

class CsvDeckParser {
 public:
  explicit CsvDeckParser(Deck& deck);

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

  bool appendField();
  bool finishRecord();
  bool finishInputRecord();
  bool hasCurrentRecord() const;
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
  bool headerSeen = false;
  bool finished = false;
};

struct DeckParseResult {
  Deck deck;
  DeckParseError error;

  bool ok() const { return error.code == DeckParseErrorCode::None; }
};

DeckParseResult parseCsv(std::string_view csv);

}
