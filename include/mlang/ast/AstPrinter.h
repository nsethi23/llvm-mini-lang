// Renders an AST as an s-expression, for --dump-ast and for golden tests
// that pin down parser output.
#ifndef MLANG_AST_ASTPRINTER_H
#define MLANG_AST_ASTPRINTER_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/ast/Stmt.h"

#include <string>

namespace mlang {

std::string printAst(const Program& program);
std::string printAst(const Expr& expr);
std::string printAst(const Stmt& stmt);

} // namespace mlang

#endif // MLANG_AST_ASTPRINTER_H
