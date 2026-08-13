// Enumerates every lexical token kind the mlang lexer can produce, plus a
// helper to render a kind as text for diagnostics and --dump-tokens output.
#ifndef MLANG_LEXER_TOKENKIND_H
#define MLANG_LEXER_TOKENKIND_H

#include <string_view>

namespace mlang {

enum class TokenKind {
  // Literals
  Identifier,
  IntLiteral,
  FloatLiteral,
  StringLiteral,

  // Keywords
  KwFn,
  KwLet,
  KwReturn,
  KwIf,
  KwElse,
  KwWhile,
  KwTrue,
  KwFalse,
  KwAs,
  KwInt,
  KwFloat,
  KwBool,

  // Punctuation
  LParen,
  RParen,
  LBrace,
  RBrace,
  Comma,
  Colon,
  Semicolon,
  Arrow,

  // Operators
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Assign,
  EqualEqual,
  BangEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  AmpAmp,
  PipePipe,
  Bang,

  // Meta
  Eof,
  Error,
};

std::string_view tokenKindName(TokenKind kind);

} // namespace mlang

#endif // MLANG_LEXER_TOKENKIND_H
