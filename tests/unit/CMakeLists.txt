add_executable(mlang_unit_tests placeholder_test.cpp)
target_link_libraries(mlang_unit_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mlang_unit_tests)
