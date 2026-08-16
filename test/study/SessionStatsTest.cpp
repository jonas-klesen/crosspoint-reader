#include <gtest/gtest.h>

#include <cstdint>

#include "SessionStats.h"

namespace {

using studycore::RecallJudgment;
using studycore::SessionStats;
using studycore::knownPercentage;
using studycore::recordJudgment;

TEST(SessionStats, StartsEmpty) {
  const SessionStats stats{};
  EXPECT_EQ(stats.reviewed, 0);
  EXPECT_EQ(stats.known, 0);
  EXPECT_EQ(stats.didNotKnow, 0);
  EXPECT_EQ(knownPercentage(stats), 0);
}

TEST(SessionStats, RecordsKnownJudgment) {
  const SessionStats stats = recordJudgment(SessionStats{}, RecallJudgment::Known);
  EXPECT_EQ(stats.reviewed, 1);
  EXPECT_EQ(stats.known, 1);
  EXPECT_EQ(stats.didNotKnow, 0);
}

TEST(SessionStats, RecordsDidNotKnowJudgment) {
  const SessionStats stats = recordJudgment(SessionStats{}, RecallJudgment::DidNotKnow);
  EXPECT_EQ(stats.reviewed, 1);
  EXPECT_EQ(stats.known, 0);
  EXPECT_EQ(stats.didNotKnow, 1);
}

TEST(SessionStats, AggregatesMixedJudgments) {
  SessionStats stats{};
  stats = recordJudgment(stats, RecallJudgment::Known);
  stats = recordJudgment(stats, RecallJudgment::Known);
  stats = recordJudgment(stats, RecallJudgment::DidNotKnow);

  EXPECT_EQ(stats.reviewed, 3);
  EXPECT_EQ(stats.known, 2);
  EXPECT_EQ(stats.didNotKnow, 1);
  EXPECT_EQ(stats.reviewed, stats.known + stats.didNotKnow);
}

TEST(SessionStats, RoundsKnownPercentageToNearestWholePercent) {
  EXPECT_EQ(knownPercentage(SessionStats{4, 3, 1}), 75);
  EXPECT_EQ(knownPercentage(SessionStats{3, 2, 1}), 67);
  EXPECT_EQ(knownPercentage(SessionStats{3, 1, 2}), 33);
  EXPECT_EQ(knownPercentage(SessionStats{1, 1, 0}), 100);
  EXPECT_EQ(knownPercentage(SessionStats{1, 0, 1}), 0);
}

TEST(SessionStats, KeepsMalformedKnownCountWithinPercentageBounds) {
  EXPECT_EQ(knownPercentage(SessionStats{3, 4, 0}), 100);
}

}  // namespace
