add_executable(StudyDeckParserTest
  StudyDeckParserTest.cpp
  ${REPO_ROOT}/lib/StudyCore/CsvDeckParser.cpp
)

target_include_directories(StudyDeckParserTest PRIVATE
  ${REPO_ROOT}/lib/StudyCore
)

target_compile_definitions(StudyDeckParserTest PRIVATE
  STUDY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(StudyDeckParserTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(StudyDeckParserTest)

add_executable(DeckRepositoryRulesTest
  DeckRepositoryRulesTest.cpp
)

target_include_directories(DeckRepositoryRulesTest PRIVATE
  ${REPO_ROOT}/src/study/storage
  ${REPO_ROOT}/lib/StudyCore
)

target_link_libraries(DeckRepositoryRulesTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(DeckRepositoryRulesTest)

add_executable(StudyFormatTest
  StudyFormatTest.cpp
)

target_include_directories(StudyFormatTest PRIVATE
  ${REPO_ROOT}/src/activities/study
)

target_link_libraries(StudyFormatTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(StudyFormatTest)
add_executable(SessionStatsTest
  SessionStatsTest.cpp
)

target_include_directories(SessionStatsTest PRIVATE
  ${REPO_ROOT}/lib/StudyCore
)

target_link_libraries(SessionStatsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(SessionStatsTest)
