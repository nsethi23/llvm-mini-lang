# Smoke test: the driver binary builds and runs.
add_test(NAME mlang_smoke_test COMMAND mlang)

# Unit tests (GoogleTest) for individual pipeline stages -- lexer/parser/etc.
add_subdirectory(unit)

# Golden-file tests: tests/golden/*.mlang + *.expected, cross-checked
# against the interpreter. See CLAUDE.md's "Build & test commands".
add_executable(golden_runner golden/golden_runner.cpp)
target_link_libraries(golden_runner PRIVATE mlang_interpreter mlang_parser mlang_ast mlang_lexer
                                             ${LLVM_LIBS})
add_test(NAME golden_tests COMMAND golden_runner ${CMAKE_SOURCE_DIR}/tests/golden)
