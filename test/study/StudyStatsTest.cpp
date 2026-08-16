#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "SessionStats.h"
#include "StudyStats.h"

namespace {

using studycore::addSession;
using studycore::hasValidInvariant;
using studycore::knownPercentage;
using studycore::RecallJudgment;
using studycore::recordJudgment;
using studycore::SessionStats;
using studycore::SessionStatsCommitGuard;
using studycore::StudyStats;

TEST(StudyStats, CommitGuardAllowsOnlyOneCommitUntilReset) {
  SessionStatsCommitGuard guard;
  EXPECT_FALSE(guard.committed());
  EXPECT_TRUE(guard.tryCommit());
  EXPECT_TRUE(guard.committed());
  EXPECT_FALSE(guard.tryCommit());

  guard.reset();
  EXPECT_FALSE(guard.committed());
  EXPECT_TRUE(guard.tryCommit());
}

TEST(StudyStats, StartsEmpty) {
  const StudyStats stats{};
  EXPECT_EQ(stats.totalReviews, 0U);
  EXPECT_EQ(stats.known, 0U);
  EXPECT_EQ(stats.didNotKnow, 0U);
  EXPECT_EQ(stats.completedSessions, 0U);
  EXPECT_TRUE(hasValidInvariant(stats));
  EXPECT_EQ(knownPercentage(stats), 0U);
}

TEST(StudyStats, AggregatesKnownSession) {
  SessionStats session{};
  session = recordJudgment(session, RecallJudgment::Known);
  session = recordJudgment(session, RecallJudgment::Known);

  const StudyStats stats = addSession(StudyStats{}, session, false);
  EXPECT_EQ(stats.totalReviews, 2U);
  EXPECT_EQ(stats.known, 2U);
  EXPECT_EQ(stats.didNotKnow, 0U);
  EXPECT_EQ(stats.completedSessions, 0U);
}

TEST(StudyStats, AggregatesDidNotKnowSession) {
  SessionStats session{};
  session = recordJudgment(session, RecallJudgment::DidNotKnow);

  const StudyStats stats = addSession(StudyStats{}, session, false);
  EXPECT_EQ(stats.totalReviews, 1U);
  EXPECT_EQ(stats.known, 0U);
  EXPECT_EQ(stats.didNotKnow, 1U);
}

TEST(StudyStats, AggregatesMixedSession) {
  SessionStats session{};
  session = recordJudgment(session, RecallJudgment::Known);
  session = recordJudgment(session, RecallJudgment::DidNotKnow);
  session = recordJudgment(session, RecallJudgment::Known);

  const StudyStats stats = addSession(StudyStats{}, session, true);
  EXPECT_EQ(stats.totalReviews, 3U);
  EXPECT_EQ(stats.known, 2U);
  EXPECT_EQ(stats.didNotKnow, 1U);
  EXPECT_EQ(stats.completedSessions, 1U);
  EXPECT_TRUE(hasValidInvariant(stats));
}

TEST(StudyStats, PartialSessionKeepsJudgmentsWithoutCompletedSession) {
  const SessionStats session{2, 1, 1};
  const StudyStats stats = addSession(StudyStats{}, session, false);

  EXPECT_EQ(stats.totalReviews, 2U);
  EXPECT_EQ(stats.known, 1U);
  EXPECT_EQ(stats.didNotKnow, 1U);
  EXPECT_EQ(stats.completedSessions, 0U);
}

TEST(StudyStats, ZeroReviewAbandonedSessionDoesNotChangeTotals) {
  const StudyStats initial{7, 4, 3, 2};
  const StudyStats stats = addSession(initial, SessionStats{}, false);
  EXPECT_EQ(stats.totalReviews, 7U);
  EXPECT_EQ(stats.known, 4U);
  EXPECT_EQ(stats.didNotKnow, 3U);
  EXPECT_EQ(stats.completedSessions, 2U);
}

TEST(StudyStats, RoundsKnownPercentageToNearestWholePercent) {
  EXPECT_EQ(knownPercentage(StudyStats{3, 1, 2, 0}), 33U);
  EXPECT_EQ(knownPercentage(StudyStats{3, 2, 1, 0}), 67U);
  EXPECT_EQ(knownPercentage(StudyStats{4, 3, 1, 0}), 75U);
  EXPECT_EQ(knownPercentage(StudyStats{1, 1, 0, 0}), 100U);
  EXPECT_EQ(knownPercentage(StudyStats{1, 0, 1, 0}), 0U);
}

TEST(StudyStats, RejectsInvalidSessionWithoutChangingTotals) {
  const StudyStats initial{7, 4, 3, 2};
  const StudyStats stats = addSession(initial, SessionStats{3, 1, 1}, true);
  EXPECT_EQ(stats.totalReviews, initial.totalReviews);
  EXPECT_EQ(stats.known, initial.known);
  EXPECT_EQ(stats.didNotKnow, initial.didNotKnow);
  EXPECT_EQ(stats.completedSessions, initial.completedSessions);
}

TEST(StudyStats, SaturatesReviewCountersWithoutBreakingInvariant) {
  constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
  const StudyStats initial{max - 1U, max - 2U, 1U, max};
  const StudyStats stats = addSession(initial, SessionStats{2, 1, 1}, true);

  EXPECT_EQ(stats.totalReviews, max);
  EXPECT_EQ(stats.known, max - 1U);
  EXPECT_EQ(stats.didNotKnow, 1U);
  EXPECT_EQ(stats.completedSessions, max);
  EXPECT_TRUE(hasValidInvariant(stats));
}

TEST(StudyStats, SaturatesUnknownCounterAtRemainingCapacity) {
  constexpr uint32_t max = std::numeric_limits<uint32_t>::max();
  const StudyStats initial{max - 1U, max - 1U, 0U, 0U};
  const StudyStats stats = addSession(initial, SessionStats{2, 0, 2}, false);

  EXPECT_EQ(stats.totalReviews, max);
  EXPECT_EQ(stats.known, max - 1U);
  EXPECT_EQ(stats.didNotKnow, 1U);
  EXPECT_TRUE(hasValidInvariant(stats));
}

TEST(StudyStats, DetectsInvariantMismatch) {
  EXPECT_FALSE(hasValidInvariant(StudyStats{4, 1, 1, 0}));
  EXPECT_TRUE(hasValidInvariant(StudyStats{4, 1, 3, 0}));
}

}  // namespace
