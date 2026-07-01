#include "mlang/parser/Parser.h"

namespace mlang {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const {
  return tokens_[pos_];
}

const Token& Parser::previous() const {
  return tokens_[pos_ - 1];
}

bool Parser::isAtEnd() const {
  return peek().kind == TokenKind::Eof;
}

const Token& Parser::advance() {
  if (!isAtEnd())
    pos_++;
  return previous();
}

bool Parser::check(TokenKind kind) const {
  return !isAtEnd() && peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
  if (!check(kind))
    return false;
  advance();
  return true;
}

const Token& Parser::expect(TokenKind kind, const std::string& message) {
  if (check(kind))
    return advance();
  error(peek(), message);
}

void Parser::error(const Token& at, const std::string& message) {
  diagnostics_.push_back(Diagnostic{at.loc, message});
  throw ParseError{};
}

TypeName Parser::parseTypeName() {
  if (match(TokenKind::KwInt))
    return TypeName::Int;
  if (match(TokenKind::KwFloat))
    return TypeName::Float;
  if (match(TokenKind::KwBool))
    return TypeName::Bool;
  error(peek(), "expected a type ('int', 'float', or 'bool')");
}

ExprPtr Parser::parseExpression() {
  try {
    ExprPtr expr = parseOr();
    if (!isAtEnd())
      error(peek(), "unexpected trailing input after expression");
    return expr;
  } catch (const ParseError&) {
    return nullptr;
  }
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
  try {
    return parseBlockRaw();
  } catch (const ParseError&) {
    return nullptr;
  }
}

std::unique_ptr<BlockStmt> Parser::parseBlockRaw() {
  SourceLocation loc = expect(TokenKind::LBrace, "expected '{' to start a block").loc;
  std::vector<StmtPtr> stmts;
  while (!check(TokenKind::RBrace) && !isAtEnd())
    stmts.push_back(parseStatement());
  expect(TokenKind::RBrace, "expected '}' to close block");
  return std::make_unique<BlockStmt>(std::move(stmts), loc);
}

StmtPtr Parser::parseStatement() {
  if (check(TokenKind::KwLet))
    return parseLetStmt(advance().loc);
  if (check(TokenKind::KwReturn))
    return parseReturnStmt(advance().loc);
  if (check(TokenKind::KwIf))
    return parseIfStmt(advance().loc);
  if (check(TokenKind::KwWhile))
    return parseWhileStmt(advance().loc);
  return parseExprStmt();
}

StmtPtr Parser::parseLetStmt(SourceLocation loc) {
  std::string name = expect(TokenKind::Identifier, "expected variable name after 'let'").lexeme;
  expect(TokenKind::Colon, "expected ':' after variable name");
  TypeName type = parseTypeName();
  expect(TokenKind::Assign, "expected '=' after type in let statement");
  ExprPtr init = parseOr();
  expect(TokenKind::Semicolon, "expected ';' after let statement");
  return std::make_unique<LetStmt>(std::move(name), type, std::move(init), loc);
}

StmtPtr Parser::parseReturnStmt(SourceLocation loc) {
  ExprPtr value;
  if (!check(TokenKind::Semicolon))
    value = parseOr();
  expect(TokenKind::Semicolon, "expected ';' after return statement");
  return std::make_unique<ReturnStmt>(std::move(value), loc);
}

StmtPtr Parser::parseIfStmt(SourceLocation loc) {
  ExprPtr cond = parseOr();
  std::unique_ptr<BlockStmt> thenBlock = parseBlockRaw();
  std::unique_ptr<BlockStmt> elseBlock;
  if (match(TokenKind::KwElse))
    elseBlock = parseBlockRaw();
  return std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock), std::move(elseBlock), loc);
}

StmtPtr Parser::parseWhileStmt(SourceLocation loc) {
  ExprPtr cond = parseOr();
  std::unique_ptr<BlockStmt> body = parseBlockRaw();
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body), loc);
}

StmtPtr Parser::parseExprStmt() {
  SourceLocation loc = peek().loc;
  ExprPtr expr = parseOr();
  expect(TokenKind::Semicolon, "expected ';' after expression statement");
  return std::make_unique<ExprStmt>(std::move(expr), loc);
}

ExprPtr Parser::parseOr() {
  ExprPtr expr = parseAnd();
  while (match(TokenKind::PipePipe)) {
    SourceLocation loc = previous().loc;
    ExprPtr rhs = parseAnd();
    expr = std::make_unique<BinaryExpr>(BinaryOp::Or, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseAnd() {
  ExprPtr expr = parseEquality();
  while (match(TokenKind::AmpAmp)) {
    SourceLocation loc = previous().loc;
    ExprPtr rhs = parseEquality();
    expr = std::make_unique<BinaryExpr>(BinaryOp::And, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseEquality() {
  ExprPtr expr = parseComparison();
  while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) {
    BinaryOp op = check(TokenKind::EqualEqual) ? BinaryOp::Eq : BinaryOp::Ne;
    SourceLocation loc = advance().loc;
    ExprPtr rhs = parseComparison();
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseComparison() {
  ExprPtr expr = parseAdditive();
  while (check(TokenKind::Less) || check(TokenKind::LessEqual) || check(TokenKind::Greater) ||
         check(TokenKind::GreaterEqual)) {
    BinaryOp op;
    if (check(TokenKind::Less))
      op = BinaryOp::Lt;
    else if (check(TokenKind::LessEqual))
      op = BinaryOp::Le;
    else if (check(TokenKind::Greater))
      op = BinaryOp::Gt;
    else
      op = BinaryOp::Ge;
    SourceLocation loc = advance().loc;
    ExprPtr rhs = parseAdditive();
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseAdditive() {
  ExprPtr expr = parseMultiplicative();
  while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
    BinaryOp op = check(TokenKind::Plus) ? BinaryOp::Add : BinaryOp::Sub;
    SourceLocation loc = advance().loc;
    ExprPtr rhs = parseMultiplicative();
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseMultiplicative() {
  ExprPtr expr = parseUnary();
  while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
    BinaryOp op = check(TokenKind::Star)
                      ? BinaryOp::Mul
                      : (check(TokenKind::Slash) ? BinaryOp::Div : BinaryOp::Mod);
    SourceLocation loc = advance().loc;
    ExprPtr rhs = parseUnary();
    expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
  }
  return expr;
}

ExprPtr Parser::parseUnary() {
  if (check(TokenKind::Minus) || check(TokenKind::Bang)) {
    UnaryOp op = check(TokenKind::Minus) ? UnaryOp::Neg : UnaryOp::Not;
    SourceLocation loc = advance().loc;
    ExprPtr operand = parseUnary();
    return std::make_unique<UnaryExpr>(op, std::move(operand), loc);
  }
  return parseCast();
}

ExprPtr Parser::parseCast() {
  ExprPtr expr = parsePrimary();
  while (match(TokenKind::KwAs)) {
    SourceLocation loc = previous().loc;
    TypeName target = parseTypeName();
    expr = std::make_unique<CastExpr>(std::move(expr), target, loc);
  }
  return expr;
}

ExprPtr Parser::parsePrimary() {
  if (check(TokenKind::IntLiteral)) {
    const Token& tok = advance();
    return std::make_unique<IntLiteralExpr>(std::stoll(tok.lexeme), tok.loc);
  }
  if (check(TokenKind::FloatLiteral)) {
    const Token& tok = advance();
    return std::make_unique<FloatLiteralExpr>(std::stod(tok.lexeme), tok.loc);
  }
  if (check(TokenKind::StringLiteral)) {
    const Token& tok = advance();
    return std::make_unique<StringLiteralExpr>(tok.lexeme, tok.loc);
  }
  if (match(TokenKind::KwTrue))
    return std::make_unique<BoolLiteralExpr>(true, previous().loc);
  if (match(TokenKind::KwFalse))
    return std::make_unique<BoolLiteralExpr>(false, previous().loc);
  if (check(TokenKind::Identifier)) {
    const Token& tok = advance();
    if (match(TokenKind::LParen)) {
      std::vector<ExprPtr> args = parseArgs();
      expect(TokenKind::RParen, "expected ')' after call arguments");
      return std::make_unique<CallExpr>(tok.lexeme, std::move(args), tok.loc);
    }
    return std::make_unique<IdentifierExpr>(tok.lexeme, tok.loc);
  }
  if (match(TokenKind::LParen)) {
    ExprPtr expr = parseOr();
    expect(TokenKind::RParen, "expected ')' after expression");
    return expr;
  }

  error(peek(), "expected an expression");
}

std::vector<ExprPtr> Parser::parseArgs() {
  std::vector<ExprPtr> args;
  if (check(TokenKind::RParen))
    return args;
  args.push_back(parseOr());
  while (match(TokenKind::Comma))
    args.push_back(parseOr());
  return args;
}

} // namespace mlang
