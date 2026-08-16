#include "CsvDeckParser.h"

#include <algorithm>
#include <utility>

namespace studycore {
namespace {

constexpr std::string_view UTF8_BOM = "\xEF\xBB\xBF";
constexpr std::string_view CARD_ID_HEADER = "card_id";
constexpr std::string_view FRONT_HEADER = "front";
constexpr std::string_view BACK_HEADER = "back";

}  // namespace

CsvDeckParser::CsvDeckParser(Deck& deck) : deck(deck) {
  deck.cards.clear();
  deck.cards.reserve(Deck::MAX_CARDS_PER_DECK);
}

bool CsvDeckParser::isAsciiWhitespace(const char value) { return value == ' ' || value == '\t'; }

bool CsvDeckParser::isBlankField(const std::string& value) {
  return value.empty() || std::all_of(value.begin(), value.end(), isAsciiWhitespace);
}

std::size_t CsvDeckParser::fieldLimitFor(const std::size_t fieldIndex) {
  // The header row uses the same columns; a header field that exceeds the
  // column limit can never match the expected literals anyway.
  if (fieldIndex == 0) return Card::MAX_ID_BYTES;
  return Card::MAX_TEXT_BYTES;
}

bool CsvDeckParser::canAppendFieldByte() const { return field.size() < fieldLimitFor(fieldCount); }

bool CsvDeckParser::appendField() {
  if (fieldCount >= 4) return setError(DeckParseErrorCode::WrongFieldCount, row);

  fields[fieldCount++] = std::move(field);
  field.clear();
  return true;
}

bool CsvDeckParser::hasCurrentRecord() const {
  return fieldCount != 0 || state != State::FieldStart || rowHadComma || !field.empty();
}

bool CsvDeckParser::hasPendingFinalField() const { return state != State::FieldStart || rowHadComma; }

bool CsvDeckParser::isBlankRecord() const {
  return fieldCount == 0 || (fieldCount == 1 && !rowHadComma && !rowHadQuote && isBlankField(fields[0]));
}

bool CsvDeckParser::processHeader() {
  if (fieldCount != 3 || rowHadQuote) return setError(DeckParseErrorCode::InvalidHeader, row);

  std::string firstField = fields[0];
  if (firstField.compare(0, UTF8_BOM.size(), UTF8_BOM) == 0) firstField.erase(0, UTF8_BOM.size());

  if (firstField != CARD_ID_HEADER || fields[1] != FRONT_HEADER || fields[2] != BACK_HEADER) {
    return setError(DeckParseErrorCode::InvalidHeader, row);
  }

  headerSeen = true;
  return true;
}

bool CsvDeckParser::setDuplicateError(const std::string& id) {
  if (parseError.code != DeckParseErrorCode::None) return false;

  parseError.code = DeckParseErrorCode::DuplicateCardId;
  parseError.row = row;
  parseError.duplicateId = id;
  deck.cards.clear();
  return false;
}

bool CsvDeckParser::processCard() {
  if (fieldCount != 3) return setError(DeckParseErrorCode::WrongFieldCount, row);
  if (isBlankField(fields[0])) return setError(DeckParseErrorCode::EmptyCardId, row);
  if (isBlankField(fields[1])) return setError(DeckParseErrorCode::EmptyFront, row);
  if (isBlankField(fields[2])) return setError(DeckParseErrorCode::EmptyBack, row);

  for (const Card& card : deck.cards) {
    if (card.id == fields[0]) return setDuplicateError(fields[0]);
  }

  if (deck.cards.size() >= Deck::MAX_CARDS_PER_DECK) {
    return setError(DeckParseErrorCode::TooManyCards, row);
  }

  deck.cards.push_back(Card{std::move(fields[0]), std::move(fields[1]), std::move(fields[2])});
  return true;
}

bool CsvDeckParser::finishRecord() {
  if (state == State::InQuotedField) return setError(DeckParseErrorCode::MalformedCsv, row);
  if (hasPendingFinalField()) {
    if (!appendField()) return false;
  }

  const bool valid = isBlankRecord() ? true : (headerSeen ? processCard() : processHeader());
  if (!valid) return false;

  resetRecord();
  ++row;
  return true;
}

bool CsvDeckParser::finishInputRecord() {
  if (!hasCurrentRecord()) return true;
  return finishRecord();
}

bool CsvDeckParser::setError(const DeckParseErrorCode code, const std::size_t errorRow) {
  if (parseError.code != DeckParseErrorCode::None) return false;

  parseError.code = code;
  parseError.row = errorRow;
  parseError.duplicateId.clear();
  deck.cards.clear();
  return false;
}

void CsvDeckParser::resetRecord() {
  field.clear();
  for (std::size_t i = 0; i < fieldCount; ++i) fields[i].clear();
  fieldCount = 0;
  state = State::FieldStart;
  rowHadComma = false;
  rowHadQuote = false;
}

bool CsvDeckParser::feed(const std::string_view bytes) {
  if (parseError.code != DeckParseErrorCode::None || finished) return ok();

  for (const char value : bytes) {
    if (pendingCarriageReturn) {
      if (value != '\n') return setError(DeckParseErrorCode::MalformedCsv, row);
      pendingCarriageReturn = false;
      if (!finishRecord()) return false;
      continue;
    }

    if (value == '\r') {
      if (state == State::InQuotedField) return setError(DeckParseErrorCode::MalformedCsv, row);
      pendingCarriageReturn = true;
      continue;
    }

    if (value == '\n') {
      if (state == State::InQuotedField) return setError(DeckParseErrorCode::MalformedCsv, row);
      if (!finishRecord()) return false;
      continue;
    }

    switch (state) {
      case State::FieldStart:
        if (value == ',') {
          rowHadComma = true;
          if (!appendField()) return false;
        } else if (value == '"') {
          rowHadQuote = true;
          state = State::InQuotedField;
        } else {
          if (!canAppendFieldByte()) return setError(DeckParseErrorCode::FieldTooLong, row);
          field += value;
          state = State::InUnquotedField;
        }
        break;

      case State::InUnquotedField:
        if (value == ',') {
          rowHadComma = true;
          if (!appendField()) return false;
          state = State::FieldStart;
        } else if (value == '"') {
          return setError(DeckParseErrorCode::MalformedCsv, row);
        } else {
          if (!canAppendFieldByte()) return setError(DeckParseErrorCode::FieldTooLong, row);
          field += value;
        }
        break;

      case State::InQuotedField:
        if (value == '"') {
          state = State::AfterQuotedQuote;
        } else {
          if (!canAppendFieldByte()) return setError(DeckParseErrorCode::FieldTooLong, row);
          field += value;
        }
        break;

      case State::AfterQuotedQuote:
        if (value == '"') {
          if (!canAppendFieldByte()) return setError(DeckParseErrorCode::FieldTooLong, row);
          field += value;
          state = State::InQuotedField;
        } else if (value == ',') {
          rowHadComma = true;
          if (!appendField()) return false;
          state = State::FieldStart;
        } else {
          return setError(DeckParseErrorCode::MalformedCsv, row);
        }
        break;
    }
  }

  return ok();
}

bool CsvDeckParser::finish() {
  if (finished) return ok();
  finished = true;

  if (parseError.code != DeckParseErrorCode::None) return false;
  if (pendingCarriageReturn || state == State::InQuotedField) {
    return setError(DeckParseErrorCode::UnexpectedEndOfInput, row);
  }
  if (!finishInputRecord()) return false;
  if (!headerSeen) return setError(DeckParseErrorCode::InvalidHeader, 1);
  return true;
}

DeckParseResult parseCsv(const std::string_view csv) {
  DeckParseResult result;
  CsvDeckParser parser(result.deck);
  const bool success = parser.feed(csv) && parser.finish();
  result.error = parser.error();
  if (!success) result.deck.cards.clear();
  return result;
}

}  // namespace studycore
