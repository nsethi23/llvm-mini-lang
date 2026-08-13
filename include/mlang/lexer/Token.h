// Defines Token and SourceLocation, the units the lexer produces and the
// parser (from M2) consumes.
#ifndef MLANG_LEXER_TOKEN_H
#define MLANG_LEXER_TOKEN_H

#include "mlang/lexer/TokenKind.h"

#include <string>

namespace mlang {

// 1-based line and column, matching how editors and compiler diagnostics
// conventionally report source positions.
struct SourceLocation {
  int line = 1;
  int column = 1;
};

struct Token {
  TokenKind kind = TokenKind::Eof;
  std::string lexeme;
  SourceLocation loc;
};

} // namespace mlang

#endif // MLANG_LEXER_TOKEN_H
