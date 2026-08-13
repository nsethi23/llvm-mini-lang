// Expression AST nodes. Each node carries a SourceLocation so later stages
// (sema, codegen) can report diagnostics that point back at the source.
#ifndef MLANG_AST_EXPR_H
#define MLANG_AST_EXPR_H

#include "mlang/ast/Type.h"
#include "mlang/lexer/Token.h"

#include <memory>
#include <string>
#include <vector>

namespace mlang {

enum class ExprKind {
  IntLiteral,
  FloatLiteral,
  BoolLiteral,
  StringLiteral,
  Identifier,
  Unary,
  Binary,
  Call,
  Cast,
};

enum class UnaryOp { Neg, Not };

enum class BinaryOp { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge, And, Or };

std::string_view unaryOpName(UnaryOp op);
std::string_view binaryOpName(BinaryOp op);

struct Expr {
  ExprKind kind;
  SourceLocation loc;
  virtual ~Expr() = default;

protected:
  Expr(ExprKind kind, SourceLocation loc) : kind(kind), loc(loc) {}
};

using ExprPtr = std::unique_ptr<Expr>;

struct IntLiteralExpr : Expr {
  int64_t value;
  IntLiteralExpr(int64_t value, SourceLocation loc)
      : Expr(ExprKind::IntLiteral, loc), value(value) {}
};

struct FloatLiteralExpr : Expr {
  double value;
  FloatLiteralExpr(double value, SourceLocation loc)
      : Expr(ExprKind::FloatLiteral, loc), value(value) {}
};

struct BoolLiteralExpr : Expr {
  bool value;
  BoolLiteralExpr(bool value, SourceLocation loc)
      : Expr(ExprKind::BoolLiteral, loc), value(value) {}
};

struct StringLiteralExpr : Expr {
  std::string value;
  StringLiteralExpr(std::string value, SourceLocation loc)
      : Expr(ExprKind::StringLiteral, loc), value(std::move(value)) {}
};

struct IdentifierExpr : Expr {
  std::string name;
  IdentifierExpr(std::string name, SourceLocation loc)
      : Expr(ExprKind::Identifier, loc), name(std::move(name)) {}
};

struct UnaryExpr : Expr {
  UnaryOp op;
  ExprPtr operand;
  UnaryExpr(UnaryOp op, ExprPtr operand, SourceLocation loc)
      : Expr(ExprKind::Unary, loc), op(op), operand(std::move(operand)) {}
};

struct BinaryExpr : Expr {
  BinaryOp op;
  ExprPtr lhs;
  ExprPtr rhs;
  BinaryExpr(BinaryOp op, ExprPtr lhs, ExprPtr rhs, SourceLocation loc)
      : Expr(ExprKind::Binary, loc), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

struct CallExpr : Expr {
  std::string callee;
  std::vector<ExprPtr> args;
  CallExpr(std::string callee, std::vector<ExprPtr> args, SourceLocation loc)
      : Expr(ExprKind::Call, loc), callee(std::move(callee)), args(std::move(args)) {}
};

struct CastExpr : Expr {
  ExprPtr operand;
  TypeName targetType;
  CastExpr(ExprPtr operand, TypeName targetType, SourceLocation loc)
      : Expr(ExprKind::Cast, loc), operand(std::move(operand)), targetType(targetType) {}
};

} // namespace mlang

#endif // MLANG_AST_EXPR_H
