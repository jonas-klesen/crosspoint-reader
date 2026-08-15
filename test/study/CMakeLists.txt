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
