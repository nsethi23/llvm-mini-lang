#include "mlang/sema/Sema.h"

namespace mlang {

Sema::Sema(const Program& program) : program_(program) {
  for (const FunctionDecl& fn : program_.functions)
    functions_[fn.name] = &fn;
}

void Sema::error(SourceLocation loc, std::string message) {
  diags_.push_back(Diagnostic{loc, std::move(message)});
}

SemaType Sema::checkExpr(const Expr& expr, Scope& scope) {
  switch (expr.kind) {
  case ExprKind::IntLiteral:
    return SemaType::Int;
  case ExprKind::FloatLiteral:
    return SemaType::Float;
  case ExprKind::BoolLiteral:
    return SemaType::Bool;
  case ExprKind::StringLiteral:
    return SemaType::String;
  case ExprKind::Identifier: {
    const auto& id = static_cast<const IdentifierExpr&>(expr);
    const SemaType* type = scope.find(id.name);
    if (!type) {
      error(expr.loc, "undefined variable '" + id.name + "'");
      return SemaType::Error;
    }
    return *type;
  }
  case ExprKind::Unary:
    return checkUnary(static_cast<const UnaryExpr&>(expr), scope);
  case ExprKind::Binary:
    return checkBinary(static_cast<const BinaryExpr&>(expr), scope);
  case ExprKind::Cast:
    return checkCast(static_cast<const CastExpr&>(expr), scope);
  case ExprKind::Call:
    return checkCall(static_cast<const CallExpr&>(expr), scope);
  }
  error(expr.loc, "internal error: unknown expression kind");
  return SemaType::Error;
}

SemaType Sema::checkUnary(const UnaryExpr& expr, Scope& scope) {
  SemaType operand = checkExpr(*expr.operand, scope);
  if (operand == SemaType::Error)
    return SemaType::Error;

  if (expr.op == UnaryOp::Neg) {
    if (operand == SemaType::Int || operand == SemaType::Float)
      return operand;
    error(expr.loc,
          "unary '-' requires int or float, got " + std::string(semaTypeName(operand)));
    return SemaType::Error;
  }

  if (operand == SemaType::Bool)
    return SemaType::Bool;
  error(expr.loc, "unary '!' requires bool, got " + std::string(semaTypeName(operand)));
  return SemaType::Error;
}

SemaType Sema::checkBinary(const BinaryExpr& expr, Scope& scope) {
  SemaType lhs = checkExpr(*expr.lhs, scope);
  SemaType rhs = checkExpr(*expr.rhs, scope);
  if (lhs == SemaType::Error || rhs == SemaType::Error)
    return SemaType::Error;

  if (expr.op == BinaryOp::And || expr.op == BinaryOp::Or) {
    if (lhs != SemaType::Bool) {
      error(expr.loc, "operand of '&&'/'||' must be bool, got " + std::string(semaTypeName(lhs)));
      return SemaType::Error;
    }
    if (rhs != SemaType::Bool) {
      error(expr.loc, "operand of '&&'/'||' must be bool, got " + std::string(semaTypeName(rhs)));
      return SemaType::Error;
    }
    return SemaType::Bool;
  }

  if (expr.op == BinaryOp::Eq || expr.op == BinaryOp::Ne) {
    if (lhs != rhs) {
      error(expr.loc, "type mismatch: '" + std::string(binaryOpName(expr.op)) +
                           "' requires operands of the same type, got " +
                           std::string(semaTypeName(lhs)) + " and " +
                           std::string(semaTypeName(rhs)));
      return SemaType::Error;
    }
    return SemaType::Bool;
  }

  bool isComparison = expr.op == BinaryOp::Lt || expr.op == BinaryOp::Le ||
                       expr.op == BinaryOp::Gt || expr.op == BinaryOp::Ge;
  if (lhs != rhs || (lhs != SemaType::Int && lhs != SemaType::Float)) {
    error(expr.loc, "type mismatch: '" + std::string(binaryOpName(expr.op)) +
                         "' requires two ints or two floats, got " +
                         std::string(semaTypeName(lhs)) + " and " +
                         std::string(semaTypeName(rhs)));
    return SemaType::Error;
  }
  return isComparison ? SemaType::Bool : lhs;
}

SemaType Sema::checkCast(const CastExpr& expr, Scope& scope) {
  SemaType operand = checkExpr(*expr.operand, scope);
  if (operand == SemaType::Error)
    return SemaType::Error;

  SemaType target = toSemaType(expr.targetType);
  bool ok = false;
  switch (target) {
  case SemaType::Int:
  case SemaType::Float:
    ok = operand == SemaType::Int || operand == SemaType::Float;
    break;
  case SemaType::Bool:
    ok = operand == SemaType::Bool;
    break;
  default:
    break;
  }
  if (!ok) {
    error(expr.loc, "unsupported cast from " + std::string(semaTypeName(operand)) + " to " +
                         std::string(semaTypeName(target)));
    return SemaType::Error;
  }
  return target;
}

SemaType Sema::checkCall(const CallExpr& expr, Scope& scope) {
  std::vector<SemaType> argTypes;
  argTypes.reserve(expr.args.size());
  for (const ExprPtr& arg : expr.args)
    argTypes.push_back(checkExpr(*arg, scope));

  if (expr.callee == "print") {
    if (expr.args.size() != 1) {
      error(expr.loc, "'print' expects 1 argument, got " + std::to_string(expr.args.size()));
      return SemaType::Error;
    }
    return SemaType::Int; // matches Interpreter::callBuiltin's `print` return value
  }

  auto it = functions_.find(expr.callee);
  if (it == functions_.end()) {
    error(expr.loc, "undefined function '" + expr.callee + "'");
    return SemaType::Error;
  }

  const FunctionDecl& fn = *it->second;
  if (argTypes.size() != fn.params.size()) {
    error(expr.loc, "'" + fn.name + "' expects " + std::to_string(fn.params.size()) +
                         " argument(s), got " + std::to_string(argTypes.size()));
    return SemaType::Error;
  }

  bool argsOk = true;
  for (size_t i = 0; i < fn.params.size(); i++) {
    SemaType expected = toSemaType(fn.params[i].type);
    if (argTypes[i] != SemaType::Error && argTypes[i] != expected) {
      error(expr.args[i]->loc, "argument " + std::to_string(i + 1) + " to '" + fn.name +
                                    "' expects " + std::string(semaTypeName(expected)) +
                                    ", got " + std::string(semaTypeName(argTypes[i])));
      argsOk = false;
    }
  }
  return argsOk ? toSemaType(fn.returnType) : SemaType::Error;
}

} // namespace mlang
