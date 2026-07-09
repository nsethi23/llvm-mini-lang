#include "mlang/interpreter/Interpreter.h"

#include <cmath>

namespace mlang {

Interpreter::Interpreter(const Program& program, llvm::raw_ostream& out)
    : program_(program), out_(out) {}

void Interpreter::error(SourceLocation loc, const std::string& message) {
  throw RuntimeError{message, loc};
}

Value Interpreter::evaluate(const Expr& expr, Environment& env) {
  switch (expr.kind) {
  case ExprKind::IntLiteral:
    return Value{static_cast<const IntLiteralExpr&>(expr).value};
  case ExprKind::FloatLiteral:
    return Value{static_cast<const FloatLiteralExpr&>(expr).value};
  case ExprKind::BoolLiteral:
    return Value{static_cast<const BoolLiteralExpr&>(expr).value};
  case ExprKind::StringLiteral:
    return Value{static_cast<const StringLiteralExpr&>(expr).value};
  case ExprKind::Identifier: {
    const auto& id = static_cast<const IdentifierExpr&>(expr);
    const Value* v = env.find(id.name);
    if (!v)
      error(expr.loc, "undefined variable '" + id.name + "'");
    return *v;
  }
  case ExprKind::Unary: {
    const auto& u = static_cast<const UnaryExpr&>(expr);
    Value operand = evaluate(*u.operand, env);
    return evaluateUnary(u.op, operand, expr.loc);
  }
  case ExprKind::Binary: {
    const auto& b = static_cast<const BinaryExpr&>(expr);
    if (b.op == BinaryOp::And || b.op == BinaryOp::Or) {
      Value lhs = evaluate(*b.lhs, env);
      if (!std::holds_alternative<bool>(lhs))
        error(expr.loc,
              "operand of '&&'/'||' must be bool, got " + std::string(valueTypeName(lhs)));
      bool lb = std::get<bool>(lhs);
      if (b.op == BinaryOp::And && !lb)
        return Value{false}; // short-circuit: rhs never evaluated
      if (b.op == BinaryOp::Or && lb)
        return Value{true}; // short-circuit: rhs never evaluated
      Value rhs = evaluate(*b.rhs, env);
      if (!std::holds_alternative<bool>(rhs))
        error(expr.loc,
              "operand of '&&'/'||' must be bool, got " + std::string(valueTypeName(rhs)));
      return rhs;
    }
    Value lhs = evaluate(*b.lhs, env);
    Value rhs = evaluate(*b.rhs, env);
    return evaluateBinary(b.op, lhs, rhs, expr.loc);
  }
  case ExprKind::Cast: {
    const auto& c = static_cast<const CastExpr&>(expr);
    Value operand = evaluate(*c.operand, env);
    return evaluateCast(operand, c.targetType, expr.loc);
  }
  case ExprKind::Call:
    error(expr.loc, "function calls not yet supported");
  }
  error(expr.loc, "internal error: unknown expression kind");
}

void Interpreter::executeBlock(const BlockStmt& block, Environment& parentEnv) {
  Environment scope(&parentEnv);
  for (const StmtPtr& s : block.stmts)
    execute(*s, scope);
}

void Interpreter::execute(const Stmt& stmt, Environment& env) {
  switch (stmt.kind) {
  case StmtKind::Let: {
    const auto& let = static_cast<const LetStmt&>(stmt);
    env.define(let.name, evaluate(*let.init, env));
    return;
  }
  case StmtKind::Assign: {
    const auto& assign = static_cast<const AssignStmt&>(stmt);
    Value value = evaluate(*assign.value, env);
    if (!env.assign(assign.name, value))
      error(stmt.loc, "undefined variable '" + assign.name + "' in assignment");
    return;
  }
  case StmtKind::Return: {
    const auto& ret = static_cast<const ReturnStmt&>(stmt);
    Value value = ret.value ? evaluate(*ret.value, env) : Value{int64_t{0}};
    throw ReturnSignal{std::move(value)};
  }
  case StmtKind::Expr: {
    evaluate(*static_cast<const ExprStmt&>(stmt).expr, env);
    return;
  }
  case StmtKind::If: {
    const auto& ifs = static_cast<const IfStmt&>(stmt);
    Value cond = evaluate(*ifs.cond, env);
    if (!std::holds_alternative<bool>(cond))
      error(stmt.loc, "if condition must be bool, got " + std::string(valueTypeName(cond)));
    if (std::get<bool>(cond))
      executeBlock(*ifs.thenBlock, env);
    else if (ifs.elseBlock)
      executeBlock(*ifs.elseBlock, env);
    return;
  }
  case StmtKind::While: {
    const auto& whileStmt = static_cast<const WhileStmt&>(stmt);
    while (true) {
      Value cond = evaluate(*whileStmt.cond, env);
      if (!std::holds_alternative<bool>(cond))
        error(stmt.loc, "while condition must be bool, got " + std::string(valueTypeName(cond)));
      if (!std::get<bool>(cond))
        break;
      executeBlock(*whileStmt.body, env);
    }
    return;
  }
  case StmtKind::Block:
    executeBlock(static_cast<const BlockStmt&>(stmt), env);
    return;
  }
  error(stmt.loc, "internal error: unknown statement kind");
}

Value Interpreter::evaluateBinary(BinaryOp op, const Value& lhs, const Value& rhs,
                                  SourceLocation loc) {
  if (op == BinaryOp::Eq)
    return Value{lhs == rhs};
  if (op == BinaryOp::Ne)
    return Value{!(lhs == rhs)};

  if (std::holds_alternative<int64_t>(lhs) && std::holds_alternative<int64_t>(rhs)) {
    int64_t a = std::get<int64_t>(lhs);
    int64_t b = std::get<int64_t>(rhs);
    switch (op) {
    case BinaryOp::Add:
      return Value{a + b};
    case BinaryOp::Sub:
      return Value{a - b};
    case BinaryOp::Mul:
      return Value{a * b};
    case BinaryOp::Div:
      if (b == 0)
        error(loc, "division by zero");
      return Value{a / b};
    case BinaryOp::Mod:
      if (b == 0)
        error(loc, "division by zero");
      return Value{a % b};
    case BinaryOp::Lt:
      return Value{a < b};
    case BinaryOp::Le:
      return Value{a <= b};
    case BinaryOp::Gt:
      return Value{a > b};
    case BinaryOp::Ge:
      return Value{a >= b};
    default:
      break;
    }
  } else if (std::holds_alternative<double>(lhs) && std::holds_alternative<double>(rhs)) {
    double a = std::get<double>(lhs);
    double b = std::get<double>(rhs);
    switch (op) {
    case BinaryOp::Add:
      return Value{a + b};
    case BinaryOp::Sub:
      return Value{a - b};
    case BinaryOp::Mul:
      return Value{a * b};
    case BinaryOp::Div:
      return Value{a / b};
    case BinaryOp::Mod:
      return Value{std::fmod(a, b)};
    case BinaryOp::Lt:
      return Value{a < b};
    case BinaryOp::Le:
      return Value{a <= b};
    case BinaryOp::Gt:
      return Value{a > b};
    case BinaryOp::Ge:
      return Value{a >= b};
    default:
      break;
    }
  }

  error(loc, "type mismatch: '" + std::string(binaryOpName(op)) +
                 "' requires two ints or two floats, got " + std::string(valueTypeName(lhs)) +
                 " and " + std::string(valueTypeName(rhs)));
}

Value Interpreter::evaluateUnary(UnaryOp op, const Value& operand, SourceLocation loc) {
  if (op == UnaryOp::Neg) {
    if (std::holds_alternative<int64_t>(operand))
      return Value{-std::get<int64_t>(operand)};
    if (std::holds_alternative<double>(operand))
      return Value{-std::get<double>(operand)};
    error(loc, "unary '-' requires int or float, got " + std::string(valueTypeName(operand)));
  }

  if (std::holds_alternative<bool>(operand))
    return Value{!std::get<bool>(operand)};
  error(loc, "unary '!' requires bool, got " + std::string(valueTypeName(operand)));
}

Value Interpreter::evaluateCast(const Value& operand, TypeName target, SourceLocation loc) {
  switch (target) {
  case TypeName::Int:
    if (std::holds_alternative<int64_t>(operand))
      return operand;
    if (std::holds_alternative<double>(operand))
      return Value{static_cast<int64_t>(std::get<double>(operand))};
    break;
  case TypeName::Float:
    if (std::holds_alternative<double>(operand))
      return operand;
    if (std::holds_alternative<int64_t>(operand))
      return Value{static_cast<double>(std::get<int64_t>(operand))};
    break;
  case TypeName::Bool:
    if (std::holds_alternative<bool>(operand))
      return operand;
    break;
  }
  error(loc, "unsupported cast from " + std::string(valueTypeName(operand)) + " to " +
                 std::string(typeName(target)));
}

} // namespace mlang
