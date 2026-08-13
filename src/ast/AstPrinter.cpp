#include "mlang/ast/AstPrinter.h"

#include <sstream>

namespace mlang {

namespace {

std::string printExpr(const Expr& expr) {
  switch (expr.kind) {
  case ExprKind::IntLiteral:
    return "(int " + std::to_string(static_cast<const IntLiteralExpr&>(expr).value) + ")";
  case ExprKind::FloatLiteral:
    return "(float " + std::to_string(static_cast<const FloatLiteralExpr&>(expr).value) + ")";
  case ExprKind::BoolLiteral:
    return std::string("(bool ") +
           (static_cast<const BoolLiteralExpr&>(expr).value ? "true" : "false") + ")";
  case ExprKind::StringLiteral:
    return "(str \"" + static_cast<const StringLiteralExpr&>(expr).value + "\")";
  case ExprKind::Identifier:
    return "(id " + static_cast<const IdentifierExpr&>(expr).name + ")";
  case ExprKind::Unary: {
    const auto& u = static_cast<const UnaryExpr&>(expr);
    return "(unary " + std::string(unaryOpName(u.op)) + " " + printExpr(*u.operand) + ")";
  }
  case ExprKind::Binary: {
    const auto& b = static_cast<const BinaryExpr&>(expr);
    return "(binary " + std::string(binaryOpName(b.op)) + " " + printExpr(*b.lhs) + " " +
           printExpr(*b.rhs) + ")";
  }
  case ExprKind::Call: {
    const auto& c = static_cast<const CallExpr&>(expr);
    std::string out = "(call " + c.callee;
    for (const ExprPtr& arg : c.args)
      out += " " + printExpr(*arg);
    return out + ")";
  }
  case ExprKind::Cast: {
    const auto& c = static_cast<const CastExpr&>(expr);
    return "(cast " + printExpr(*c.operand) + " " + std::string(typeName(c.targetType)) + ")";
  }
  }
  return "(?)";
}

std::string indentStr(int indent) {
  return std::string(static_cast<size_t>(indent) * 2, ' ');
}

std::string printStmt(const Stmt& stmt, int indent) {
  std::string pad = indentStr(indent);
  switch (stmt.kind) {
  case StmtKind::Let: {
    const auto& let = static_cast<const LetStmt&>(stmt);
    return pad + "(let " + let.name + " " + std::string(typeName(let.type)) + " " +
           printExpr(*let.init) + ")";
  }
  case StmtKind::Return: {
    const auto& ret = static_cast<const ReturnStmt&>(stmt);
    if (ret.value)
      return pad + "(return " + printExpr(*ret.value) + ")";
    return pad + "(return)";
  }
  case StmtKind::If: {
    const auto& ifs = static_cast<const IfStmt&>(stmt);
    std::string out = pad + "(if " + printExpr(*ifs.cond) + "\n";
    out += printStmt(*ifs.thenBlock, indent + 1);
    if (ifs.elseBlock) {
      out += "\n" + printStmt(*ifs.elseBlock, indent + 1);
    }
    return out + ")";
  }
  case StmtKind::While: {
    const auto& whileStmt = static_cast<const WhileStmt&>(stmt);
    return pad + "(while " + printExpr(*whileStmt.cond) + "\n" +
           printStmt(*whileStmt.body, indent + 1) + ")";
  }
  case StmtKind::Expr:
    return pad + "(exprstmt " + printExpr(*static_cast<const ExprStmt&>(stmt).expr) + ")";
  case StmtKind::Block: {
    const auto& block = static_cast<const BlockStmt&>(stmt);
    std::string out = pad + "(block";
    for (const StmtPtr& s : block.stmts)
      out += "\n" + printStmt(*s, indent + 1);
    return out + ")";
  }
  }
  return pad + "(?)";
}

std::string printFunction(const FunctionDecl& fn, int indent) {
  std::string pad = indentStr(indent);
  std::string out = pad + "(fn " + fn.name + " (";
  for (size_t i = 0; i < fn.params.size(); i++) {
    if (i > 0)
      out += " ";
    out += "(" + fn.params[i].name + " " + std::string(typeName(fn.params[i].type)) + ")";
  }
  out += ") " + std::string(typeName(fn.returnType)) + "\n";
  out += printStmt(*fn.body, indent + 1);
  return out + ")";
}

} // namespace

std::string printAst(const Expr& expr) {
  return printExpr(expr);
}
std::string printAst(const Stmt& stmt) {
  return printStmt(stmt, 0);
}

std::string printAst(const Program& program) {
  std::string out = "(program";
  for (const FunctionDecl& fn : program.functions)
    out += "\n" + printFunction(fn, 1);
  return out + ")";
}

} // namespace mlang
