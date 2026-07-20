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

# Sema error golden-file tests: tests/golden/errors/*.mlang, each expected
# to produce a specific diagnostic (PRD.md M4 demo).
add_executable(sema_error_runner golden/errors/sema_error_runner.cpp)
target_link_libraries(sema_error_runner PRIVATE mlang_sema mlang_parser mlang_ast mlang_lexer
                                                 ${LLVM_LIBS})
add_test(NAME sema_error_tests COMMAND sema_error_runner ${CMAKE_SOURCE_DIR}/tests/golden/errors)

# Codegen golden-file tests: every well-typed program in tests/golden/
# must codegen to a module that passes llvm::verifyModule (PRD.md M5 demo).
add_executable(codegen_verify_runner golden/codegen_verify_runner.cpp)
target_link_libraries(codegen_verify_runner PRIVATE mlang_codegen mlang_sema mlang_parser mlang_ast
                                                     mlang_lexer ${LLVM_LIBS})
add_test(NAME codegen_verify_tests COMMAND codegen_verify_runner ${CMAKE_SOURCE_DIR}/tests/golden)
