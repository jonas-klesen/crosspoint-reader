#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "StudyStats.h"

namespace studypet {

enum class StudyStatsJsonError {
  None,
  Malformed,
  MissingField,
  DuplicateField,
  UnknownField,
  WrongFieldType,
  NegativeNumber,
  NumericOverflow,
  UnsupportedSchema,
  InvariantMismatch,
};

struct StudyStatsJsonDecodeResult {
  studycore::StudyStats stats{};
  uint32_t schema = 0;
  StudyStatsJsonError error = StudyStatsJsonError::None;

  bool ok() const noexcept { return error == StudyStatsJsonError::None; }
};

class StudyStatsJson final {
 public:
  static constexpr std::size_t MAX_FILE_BYTES = 256;
  static constexpr std::size_t MAX_ENCODED_BYTES = 160;

  static StudyStatsJsonDecodeResult decode(std::string_view json) noexcept;
  static bool encode(const studycore::StudyStats& stats, char* buffer, std::size_t bufferSize,
                     std::size_t& encodedLength) noexcept;
};

}  // namespace studypet
