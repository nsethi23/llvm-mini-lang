// Recursive-descent parser with precedence climbing for expressions.
// Statement/function/program parsing is added incrementally on top of the
// expression grammar in later commits (see PRD.md M2).
#ifndef MLANG_PARSER_PARSER_H
#define MLANG_PARSER_PARSER_H

#include "mlang/ast/Decl.h"
#include "mlang/ast/Expr.h"
#include "mlang/ast/Stmt.h"
#include "mlang/parser/Diagnostic.h"

#include <vector>

namespace mlang {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  // Parses a single expression to end-of-input. On a syntax error, records
  // a diagnostic and returns nullptr.
  ExprPtr parseExpression();

  // Parses a single "{ ... }" block. On a syntax error anywhere inside,
  // records a diagnostic and returns nullptr -- multi-error recovery within
  // a block is added in a later commit.
  std::unique_ptr<BlockStmt> parseBlock();

  // Parses a whole program (function*). On a syntax error, records a
  // diagnostic and returns whatever functions parsed successfully before
  // it; recovering to keep parsing past the error and report every syntax
  // error in the file is added in a later commit.
  Program parseProgram();

  const std::vector<Diagnostic>& diagnostics() const {
    return diagnostics_;
  }

private:
  // A private signal used to unwind out of a broken expression/statement so
  // the caller can decide how to recover. Never escapes a public entry
  // point.
  struct ParseError {};

  // Unlike the public parseBlock(), this propagates ParseError to the
  // caller instead of swallowing it -- used when a block is nested inside
  // a larger construct (if/while/function) so one error invalidates the
  // whole enclosing parse, not just the inner block.
  std::unique_ptr<BlockStmt> parseBlockRaw();

  StmtPtr parseStatement();
  StmtPtr parseLetStmt(SourceLocation loc);
  StmtPtr parseReturnStmt(SourceLocation loc);
  StmtPtr parseIfStmt(SourceLocation loc);
  StmtPtr parseWhileStmt(SourceLocation loc);
  StmtPtr parseExprStmt();

  FunctionDecl parseFunction();
  std::vector<Param> parseParams();
  Param parseParam();

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
