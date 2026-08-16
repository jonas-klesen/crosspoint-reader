#include "StudyStatsJson.h"

#include <charconv>
#include <cinttypes>
#include <cstdio>

namespace studypet {
namespace {

void skipWhitespace(std::string_view& input) noexcept {
  while (!input.empty()) {
    const char c = input.front();
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    input.remove_prefix(1);
  }
}

bool consume(std::string_view& input, const char expected) noexcept {
  skipWhitespace(input);
  if (input.empty() || input.front() != expected) return false;
  input.remove_prefix(1);
  return true;
}

bool parseKey(std::string_view& input, std::string_view& key) noexcept {
  skipWhitespace(input);
  if (input.empty() || input.front() != '"') return false;
  input.remove_prefix(1);
  const std::size_t keyStart = 0;
  for (std::size_t i = 0; i < input.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(input[i]);
    if (c == '"') {
      key = input.substr(keyStart, i - keyStart);
      input.remove_prefix(i + 1);
      return true;
    }
    if (c == '\\' || c < 0x20U) return false;
  }
  return false;
}

StudyStatsJsonError parseUnsigned(std::string_view& input, uint32_t& value) noexcept {
  skipWhitespace(input);
  if (input.empty()) return StudyStatsJsonError::Malformed;
  if (input.front() == '-') return StudyStatsJsonError::NegativeNumber;
  if (input.front() == '+' || input.front() < '0' || input.front() > '9') {
    return StudyStatsJsonError::WrongFieldType;
  }

  const std::size_t numberLengthStart = 0;
  if (input.front() == '0' && input.size() > 1 && input[1] >= '0' && input[1] <= '9') {
    return StudyStatsJsonError::Malformed;
  }
  std::size_t numberLength = 0;
  while (numberLength < input.size() && input[numberLength] >= '0' && input[numberLength] <= '9') {
    ++numberLength;
  }

  if (numberLength < input.size()) {
    const char next = input[numberLength];
    if (next == '.' || next == 'e' || next == 'E' || next == '+' || next == '-') {
      return StudyStatsJsonError::WrongFieldType;
    }
  }

  const std::string_view number = input.substr(numberLengthStart, numberLength);
  const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value, 10);
  if (parsed.ec == std::errc::result_out_of_range) return StudyStatsJsonError::NumericOverflow;
  if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()) {
    return StudyStatsJsonError::Malformed;
  }
  input.remove_prefix(numberLength);
  return StudyStatsJsonError::None;
}

}  // namespace

StudyStatsJsonDecodeResult StudyStatsJson::decode(std::string_view json) noexcept {
  StudyStatsJsonDecodeResult result;
  std::string_view input = json;
  bool schemaSeen = false;
  bool totalReviewsSeen = false;
  bool knownSeen = false;
  bool didNotKnowSeen = false;
  bool completedSessionsSeen = false;
  uint32_t schema = 0;

  if (!consume(input, '{')) {
    result.error = StudyStatsJsonError::Malformed;
    return result;
  }

  skipWhitespace(input);
  if (!input.empty() && input.front() == '}') {
    result.error = StudyStatsJsonError::MissingField;
    return result;
  }

  while (true) {
    std::string_view key;
    if (!parseKey(input, key) || !consume(input, ':')) {
      result.error = StudyStatsJsonError::Malformed;
      return result;
    }

    uint32_t value = 0;
    const StudyStatsJsonError numberError = parseUnsigned(input, value);
    if (numberError != StudyStatsJsonError::None) {
      result.error = numberError;
      return result;
    }

    if (key == "schema") {
      if (schemaSeen) {
        result.error = StudyStatsJsonError::DuplicateField;
        return result;
      }
      schemaSeen = true;
      schema = value;
    } else if (key == "total_reviews") {
      if (totalReviewsSeen) {
        result.error = StudyStatsJsonError::DuplicateField;
        return result;
      }
      totalReviewsSeen = true;
      result.stats.totalReviews = value;
    } else if (key == "known") {
      if (knownSeen) {
        result.error = StudyStatsJsonError::DuplicateField;
        return result;
      }
      knownSeen = true;
      result.stats.known = value;
    } else if (key == "did_not_know") {
      if (didNotKnowSeen) {
        result.error = StudyStatsJsonError::DuplicateField;
        return result;
      }
      didNotKnowSeen = true;
      result.stats.didNotKnow = value;
    } else if (key == "completed_sessions") {
      if (completedSessionsSeen) {
        result.error = StudyStatsJsonError::DuplicateField;
        return result;
      }
      completedSessionsSeen = true;
      result.stats.completedSessions = value;
    } else {
      result.error = StudyStatsJsonError::UnknownField;
      return result;
    }

    skipWhitespace(input);
    if (!input.empty() && input.front() == '}') {
      input.remove_prefix(1);
      break;
    }
    if (!consume(input, ',')) {
      result.error = StudyStatsJsonError::Malformed;
      return result;
    }
  }

  skipWhitespace(input);
  if (!input.empty()) {
    result.error = StudyStatsJsonError::Malformed;
    return result;
  }
  result.schema = schema;
  if (schemaSeen && schema > 1U) {
    result.error = StudyStatsJsonError::UnsupportedSchema;
    return result;
  }
  if (!schemaSeen || !totalReviewsSeen || !knownSeen || !didNotKnowSeen || !completedSessionsSeen) {
    result.error = StudyStatsJsonError::MissingField;
    return result;
  }
  if (schema != 1U) {
    result.error = StudyStatsJsonError::UnsupportedSchema;
    return result;
  }
  if (!studycore::hasValidInvariant(result.stats)) {
    result.error = StudyStatsJsonError::InvariantMismatch;
    return result;
  }
  return result;
}

bool StudyStatsJson::encode(const studycore::StudyStats& stats, char* buffer, const std::size_t bufferSize,
                            std::size_t& encodedLength) noexcept {
  encodedLength = 0;
  if (buffer == nullptr || bufferSize == 0 || !studycore::hasValidInvariant(stats)) return false;

  const int written = std::snprintf(buffer, bufferSize,
                                    "{\"schema\":1,\"total_reviews\":%" PRIu32 ",\"known\":%" PRIu32
                                    ",\"did_not_know\":%" PRIu32 ",\"completed_sessions\":%" PRIu32 "}",
                                    stats.totalReviews, stats.known, stats.didNotKnow, stats.completedSessions);
  if (written < 0 || static_cast<std::size_t>(written) >= bufferSize) return false;
  encodedLength = static_cast<std::size_t>(written);
  return true;
}

}  // namespace studypet
