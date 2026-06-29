add_library(mlang_lexer STATIC
  lexer/TokenKind.cpp
  lexer/Lexer.cpp
)
target_include_directories(mlang_lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(mlang_ast STATIC
  ast/Type.cpp
  ast/Expr.cpp
  ast/AstPrinter.cpp
)
target_include_directories(mlang_ast PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mlang_ast PUBLIC mlang_lexer)

add_library(mlang_parser STATIC
  parser/Parser.cpp
)
target_include_directories(mlang_parser PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mlang_parser PUBLIC mlang_ast mlang_lexer)
