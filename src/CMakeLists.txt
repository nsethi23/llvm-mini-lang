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

add_library(mlang_sema STATIC
  sema/SemaType.cpp
  sema/Scope.cpp
  sema/Sema.cpp
)
target_include_directories(mlang_sema PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mlang_sema PUBLIC mlang_ast)

add_library(mlang_interpreter STATIC
  interpreter/Value.cpp
  interpreter/Environment.cpp
  interpreter/Interpreter.cpp
)
target_include_directories(mlang_interpreter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mlang_interpreter PUBLIC mlang_ast ${LLVM_LIBS})

add_library(mlang_codegen STATIC
  codegen/CodeGen.cpp
)
target_include_directories(mlang_codegen PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mlang_codegen PUBLIC mlang_ast ${LLVM_LIBS})
