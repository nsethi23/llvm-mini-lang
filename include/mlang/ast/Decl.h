// Top-level declarations: function parameters, function definitions, and
// the program (a flat list of functions -- v1 has no modules/globals).
#ifndef MLANG_AST_DECL_H
#define MLANG_AST_DECL_H

#include "mlang/ast/Stmt.h"
#include "mlang/ast/Type.h"
#include "mlang/lexer/Token.h"

#include <string>
#include <vector>

namespace mlang {

struct Param {
  std::string name;
  TypeName type;
  SourceLocation loc;
};

struct FunctionDecl {
  std::string name;
  std::vector<Param> params;
  TypeName returnType;
  std::unique_ptr<BlockStmt> body;
  SourceLocation loc;
};

struct Program {
  std::vector<FunctionDecl> functions;
};

} // namespace mlang

#endif // MLANG_AST_DECL_H
