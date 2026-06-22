// Hand-written lexer: turns source text into a flat token stream. No
// external lexer generator is used, per CLAUDE.md.
#ifndef MLANG_LEXER_LEXER_H
#define MLANG_LEXER_LEXER_H

#include "mlang/lexer/Token.h"

#include <string>
#include <vector>

namespace mlang {

class Lexer {
public:
  explicit Lexer(std::string source);

  // Scans the entire source and returns its tokens, always ending in a
  // single Eof token. Illegal characters/sequences become Error tokens
  // (with a human-readable message in Token::lexeme) rather than throwing,
  // so callers can report every lexical error in a file, not just the
  // first.
  std::vector<Token> tokenize();

private:
  Token nextToken();
  Token makeToken(TokenKind kind, const std::string& lexeme, SourceLocation loc);
  Token errorToken(const std::string& message, SourceLocation loc);

  void skipWhitespaceAndComments();
  Token scanIdentifierOrKeyword();
  Token scanNumber();
  Token scanString();

  bool isAtEnd() const;
  char peek() const;
  char peekNext() const;
  char advance();
  bool match(char expected);
  SourceLocation currentLoc() const;

  std::string source_;
  size_t pos_ = 0;
  int line_ = 1;
  int column_ = 1;
};

} // namespace mlang

#endif // MLANG_LEXER_LEXER_H
