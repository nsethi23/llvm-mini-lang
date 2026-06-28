// Statement AST nodes.
#ifndef MLANG_AST_STMT_H
#define MLANG_AST_STMT_H

#include "mlang/ast/Expr.h"
#include "mlang/ast/Type.h"

#include <memory>
#include <vector>

namespace mlang {

enum class StmtKind {
  Let,
  Return,
  If,
  While,
  Expr,
  Block,
};

struct Stmt {
  StmtKind kind;
  SourceLocation loc;
  virtual ~Stmt() = default;

protected:
  Stmt(StmtKind kind, SourceLocation loc) : kind(kind), loc(loc) {}
};

using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
  std::vector<StmtPtr> stmts;
  explicit BlockStmt(std::vector<StmtPtr> stmts, SourceLocation loc)
      : Stmt(StmtKind::Block, loc), stmts(std::move(stmts)) {}
};

struct LetStmt : Stmt {
  std::string name;
  TypeName type;
  ExprPtr init;
  LetStmt(std::string name, TypeName type, ExprPtr init, SourceLocation loc)
      : Stmt(StmtKind::Let, loc), name(std::move(name)), type(type), init(std::move(init)) {}
};

struct ReturnStmt : Stmt {
  ExprPtr value; // null for bare `return;`
  ReturnStmt(ExprPtr value, SourceLocation loc)
      : Stmt(StmtKind::Return, loc), value(std::move(value)) {}
};

struct IfStmt : Stmt {
  ExprPtr cond;
  std::unique_ptr<BlockStmt> thenBlock;
  std::unique_ptr<BlockStmt> elseBlock; // null when there's no else
  IfStmt(ExprPtr cond, std::unique_ptr<BlockStmt> thenBlock, std::unique_ptr<BlockStmt> elseBlock,
         SourceLocation loc)
      : Stmt(StmtKind::If, loc), cond(std::move(cond)), thenBlock(std::move(thenBlock)),
        elseBlock(std::move(elseBlock)) {}
};

struct WhileStmt : Stmt {
  ExprPtr cond;
  std::unique_ptr<BlockStmt> body;
  WhileStmt(ExprPtr cond, std::unique_ptr<BlockStmt> body, SourceLocation loc)
      : Stmt(StmtKind::While, loc), cond(std::move(cond)), body(std::move(body)) {}
};

struct ExprStmt : Stmt {
  ExprPtr expr;
  ExprStmt(ExprPtr expr, SourceLocation loc) : Stmt(StmtKind::Expr, loc), expr(std::move(expr)) {}
};

} // namespace mlang

#endif // MLANG_AST_STMT_H
