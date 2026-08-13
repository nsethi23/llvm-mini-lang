add_executable(mlang_unit_tests
  lexer/token_kind_test.cpp
  lexer/lexer_test.cpp
  ast/ast_nodes_test.cpp
  ast/ast_printer_test.cpp
  parser/parser_expr_test.cpp
  parser/parser_stmt_test.cpp
  parser/parser_program_test.cpp
  interpreter/value_test.cpp
  interpreter/interpreter_expr_test.cpp
  interpreter/interpreter_stmt_test.cpp
  interpreter/interpreter_control_flow_test.cpp
  interpreter/interpreter_call_test.cpp
)
target_link_libraries(mlang_unit_tests PRIVATE mlang_parser mlang_ast mlang_lexer mlang_interpreter
                                                GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mlang_unit_tests)
