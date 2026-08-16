#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "StudyStatsJson.h"

namespace {

using studycore::StudyStats;
using studypet::StudyStatsJson;
using studypet::StudyStatsJsonError;

TEST(StudyStatsJson, DecodesValidSchemaOne) {
  const auto result = StudyStatsJson::decode(
      R"({"schema":1,"total_reviews":1284,"known":917,"did_not_know":367,"completed_sessions":46})");

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.schema, 1U);
  EXPECT_EQ(result.stats.totalReviews, 1284U);
  EXPECT_EQ(result.stats.known, 917U);
  EXPECT_EQ(result.stats.didNotKnow, 367U);
  EXPECT_EQ(result.stats.completedSessions, 46U);
}

TEST(StudyStatsJson, AcceptsWhitespaceAndFieldReordering) {
  const auto result = StudyStatsJson::decode(
      " {\n \"known\": 2, \"schema\": 1, \"completed_sessions\": 1,"
      " \"did_not_know\": 1, \"total_reviews\": 3 } ");

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.stats, (StudyStats{3, 2, 1, 1}));
}

TEST(StudyStatsJson, ReportsMissingRequiredField) {
  const auto result = StudyStatsJson::decode(R"({"schema":1,"total_reviews":1,"known":1,"did_not_know":0})");
  EXPECT_EQ(result.error, StudyStatsJsonError::MissingField);
}

TEST(StudyStatsJson, ReportsMalformedJson) {
  const auto result =
      StudyStatsJson::decode(R"({"schema":1,"total_reviews":1,"known":1,"did_not_know":0,"completed_sessions":1)");
  EXPECT_EQ(result.error, StudyStatsJsonError::Malformed);
}

TEST(StudyStatsJson, ReportsWrongFieldType) {
  const auto result =
      StudyStatsJson::decode(R"({"schema":"1","total_reviews":1,"known":1,"did_not_know":0,"completed_sessions":1})");
  EXPECT_EQ(result.error, StudyStatsJsonError::WrongFieldType);
}

TEST(StudyStatsJson, ReportsNegativeNumber) {
  const auto result =
      StudyStatsJson::decode(R"({"schema":1,"total_reviews":-1,"known":0,"did_not_know":0,"completed_sessions":0})");
  EXPECT_EQ(result.error, StudyStatsJsonError::NegativeNumber);
}

TEST(StudyStatsJson, ReportsNumericOverflow) {
  const auto result = StudyStatsJson::decode(
      R"({"schema":1,"total_reviews":4294967296,"known":0,"did_not_know":0,"completed_sessions":0})");
  EXPECT_EQ(result.error, StudyStatsJsonError::NumericOverflow);
}

TEST(StudyStatsJson, ReportsInvariantMismatch) {
  const auto result =
      StudyStatsJson::decode(R"({"schema":1,"total_reviews":3,"known":2,"did_not_know":2,"completed_sessions":0})");
  EXPECT_EQ(result.error, StudyStatsJsonError::InvariantMismatch);
}

TEST(StudyStatsJson, ReportsUnknownNewerSchema) {
  const auto result =
      StudyStatsJson::decode(R"({"schema":999,"total_reviews":3,"known":2,"did_not_know":1,"completed_sessions":1})");
  EXPECT_EQ(result.error, StudyStatsJsonError::UnsupportedSchema);
  EXPECT_EQ(result.schema, 999U);
}

TEST(StudyStatsJson, ReportsNewerSchemaBeforeAttemptingMigration) {
  const auto result = StudyStatsJson::decode(R"({"schema":999})");
  EXPECT_EQ(result.error, StudyStatsJsonError::UnsupportedSchema);
  EXPECT_EQ(result.schema, 999U);
}

TEST(StudyStatsJson, ReportsDuplicateAndUnknownFields) {
  const auto duplicate = StudyStatsJson::decode(
      R"({"schema":1,"schema":1,"total_reviews":0,"known":0,"did_not_know":0,"completed_sessions":0})");
  EXPECT_EQ(duplicate.error, StudyStatsJsonError::DuplicateField);

  const auto unknown = StudyStatsJson::decode(
      R"({"schema":1,"total_reviews":0,"known":0,"did_not_know":0,"completed_sessions":0,"extra":0})");
  EXPECT_EQ(unknown.error, StudyStatsJsonError::UnknownField);
}

TEST(StudyStatsJson, EncodesStableSchemaOneShape) {
  char buffer[StudyStatsJson::MAX_ENCODED_BYTES]{};
  std::size_t length = 0;
  const StudyStats stats{1284, 917, 367, 46};

  ASSERT_TRUE(StudyStatsJson::encode(stats, buffer, sizeof(buffer), length));
  EXPECT_EQ(std::string(buffer, length),
            R"({"schema":1,"total_reviews":1284,"known":917,"did_not_know":367,"completed_sessions":46})");
}

TEST(StudyStatsJson, RejectsInvalidStatsDuringEncoding) {
  char buffer[StudyStatsJson::MAX_ENCODED_BYTES]{};
  std::size_t length = 123;
  EXPECT_FALSE(StudyStatsJson::encode(StudyStats{3, 2, 2, 0}, buffer, sizeof(buffer), length));
  EXPECT_EQ(length, 0U);
}

TEST(StudyStatsJson, RejectsTooSmallEncodingBuffer) {
  char buffer[8]{};
  std::size_t length = 123;
  EXPECT_FALSE(StudyStatsJson::encode(StudyStats{1, 1, 0, 0}, buffer, sizeof(buffer), length));
  EXPECT_EQ(length, 0U);
}

}  // namespace
