add_library(mlang_lexer STATIC
  lexer/TokenKind.cpp
)
target_include_directories(mlang_lexer PUBLIC ${CMAKE_SOURCE_DIR}/include)
