// Recursive-descent parser with precedence climbing for expressions.
// Statement/function/program parsing is added incrementally on top of the
// expression grammar in later commits (see PRD.md M2).
#ifndef MLANG_PARSER_PARSER_H
#define MLANG_PARSER_PARSER_H

#include "mlang/ast/Expr.h"
#include "mlang/parser/Diagnostic.h"

#include <vector>

namespace mlang {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  // Parses a single expression to end-of-input. On a syntax error, records
  // a diagnostic and returns nullptr.
  ExprPtr parseExpression();

  const std::vector<Diagnostic>& diagnostics() const {
    return diagnostics_;
  }

private:
  // A private signal used to unwind out of a broken expression/statement so
  // the caller can decide how to recover. Never escapes a public entry
  // point.
  struct ParseError {};

  ExprPtr parseOr();
  ExprPtr parseAnd();
  ExprPtr parseEquality();
  ExprPtr parseComparison();
  ExprPtr parseAdditive();
  ExprPtr parseMultiplicative();
  ExprPtr parseUnary();
  ExprPtr parseCast();
  ExprPtr parsePrimary();
  std::vector<ExprPtr> parseArgs();
  TypeName parseTypeName();

  const Token& peek() const;
  const Token& previous() const;
  bool isAtEnd() const;
  const Token& advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  const Token& expect(TokenKind kind, const std::string& message);
  [[noreturn]] void error(const Token& at, const std::string& message);

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<Diagnostic> diagnostics_;
};

} // namespace mlang

#endif // MLANG_PARSER_PARSER_H
