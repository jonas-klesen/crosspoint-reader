#pragma once

#include <cstdint>

#include "StudyStats.h"

namespace studypet {

enum class StudyStatsLoadStatus {
  Loaded,
  Missing,
  Corrupt,
  NewerSchema,
  StorageError,
};

struct StudyStatsLoadResult {
  studycore::StudyStats stats{};
  StudyStatsLoadStatus status = StudyStatsLoadStatus::Missing;

  bool available() const noexcept {
    return status == StudyStatsLoadStatus::Loaded || status == StudyStatsLoadStatus::Missing;
  }
};

class StudyStatsStore final {
 public:
  static constexpr const char* FILE_PATH = "/.crosspoint/study/stats.json";
  static constexpr const char* TEMP_FILE_PATH = "/.crosspoint/study/stats.json.tmp";

  StudyStatsLoadResult load() const;
  bool save(const studycore::StudyStats& stats) const;
};

}  // namespace studypet
