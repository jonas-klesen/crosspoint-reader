#pragma once

#include <cstdint>

namespace studycore {

enum class RecallJudgment {
  Known,
  DidNotKnow,
};

struct SessionStats {
  uint16_t reviewed = 0;
  uint16_t known = 0;
  uint16_t didNotKnow = 0;
};

constexpr SessionStats recordJudgment(SessionStats stats, const RecallJudgment judgment) noexcept {
  ++stats.reviewed;
  if (judgment == RecallJudgment::Known) {
    ++stats.known;
  } else {
    ++stats.didNotKnow;
  }
  return stats;
}

constexpr uint8_t knownPercentage(const SessionStats& stats) noexcept {
  if (stats.reviewed == 0) return 0;

  const uint32_t boundedKnown = stats.known > stats.reviewed ? stats.reviewed : stats.known;
  const uint32_t numerator = boundedKnown * 100U + stats.reviewed / 2U;
  return static_cast<uint8_t>(numerator / stats.reviewed);
}

}  // namespace studycore
