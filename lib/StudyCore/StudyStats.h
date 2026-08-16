#pragma once

#include <cstdint>
#include <limits>

#include "SessionStats.h"

namespace studycore {

struct StudyStats {
  uint32_t totalReviews = 0;
  uint32_t known = 0;
  uint32_t didNotKnow = 0;
  uint32_t completedSessions = 0;
  friend constexpr bool operator==(const StudyStats&, const StudyStats&) = default;
};

class SessionStatsCommitGuard final {
 public:
  constexpr bool tryCommit() noexcept {
    if (committed_) return false;
    committed_ = true;
    return true;
  }

  constexpr void reset() noexcept { committed_ = false; }
  constexpr bool committed() const noexcept { return committed_; }

 private:
  bool committed_ = false;
};

constexpr bool hasValidInvariant(const StudyStats& stats) noexcept {
  const uint64_t categoryTotal = static_cast<uint64_t>(stats.known) + stats.didNotKnow;
  return categoryTotal == stats.totalReviews;
}

constexpr uint8_t knownPercentage(const StudyStats& stats) noexcept {
  if (stats.totalReviews == 0) return 0;

  const uint32_t boundedKnown = stats.known > stats.totalReviews ? stats.totalReviews : stats.known;
  const uint64_t numerator = static_cast<uint64_t>(boundedKnown) * 100U + stats.totalReviews / 2U;
  return static_cast<uint8_t>(numerator / stats.totalReviews);
}

// Adds a valid session without allowing any counter to wrap. When the review
// total reaches UINT32_MAX, new judgments are accepted in Known-then-
// Didn't-know order until the invariant-preserving capacity is full.
constexpr StudyStats addSession(StudyStats totals, const SessionStats& session, const bool completed) noexcept {
  const uint32_t sessionCategoryTotal = static_cast<uint32_t>(session.known) + session.didNotKnow;
  if (session.reviewed != sessionCategoryTotal || !hasValidInvariant(totals)) return totals;

  const uint32_t remaining = std::numeric_limits<uint32_t>::max() - totals.totalReviews;
  const uint32_t addedKnown = session.known < remaining ? session.known : remaining;
  const uint32_t remainingAfterKnown = remaining - addedKnown;
  const uint32_t addedDidNotKnow = session.didNotKnow < remainingAfterKnown ? session.didNotKnow : remainingAfterKnown;

  totals.known += addedKnown;
  totals.didNotKnow += addedDidNotKnow;
  totals.totalReviews += addedKnown + addedDidNotKnow;
  if (completed && totals.completedSessions != std::numeric_limits<uint32_t>::max()) ++totals.completedSessions;
  return totals;
}

}  // namespace studycore
