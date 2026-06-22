#include "mlang/lexer/TokenKind.h"

namespace mlang {

std::string_view tokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::Identifier:
    return "Identifier";
  case TokenKind::IntLiteral:
    return "IntLiteral";
  case TokenKind::FloatLiteral:
    return "FloatLiteral";
  case TokenKind::StringLiteral:
    return "StringLiteral";
  case TokenKind::KwFn:
    return "KwFn";
  case TokenKind::KwLet:
    return "KwLet";
  case TokenKind::KwReturn:
    return "KwReturn";
  case TokenKind::KwIf:
    return "KwIf";
  case TokenKind::KwElse:
    return "KwElse";
  case TokenKind::KwWhile:
    return "KwWhile";
  case TokenKind::KwTrue:
    return "KwTrue";
  case TokenKind::KwFalse:
    return "KwFalse";
  case TokenKind::KwAs:
    return "KwAs";
  case TokenKind::KwInt:
    return "KwInt";
  case TokenKind::KwFloat:
    return "KwFloat";
  case TokenKind::KwBool:
    return "KwBool";
  case TokenKind::LParen:
    return "LParen";
  case TokenKind::RParen:
    return "RParen";
  case TokenKind::LBrace:
    return "LBrace";
  case TokenKind::RBrace:
    return "RBrace";
  case TokenKind::Comma:
    return "Comma";
  case TokenKind::Colon:
    return "Colon";
  case TokenKind::Semicolon:
    return "Semicolon";
  case TokenKind::Arrow:
    return "Arrow";
  case TokenKind::Plus:
    return "Plus";
  case TokenKind::Minus:
    return "Minus";
  case TokenKind::Star:
    return "Star";
  case TokenKind::Slash:
    return "Slash";
  case TokenKind::Percent:
    return "Percent";
  case TokenKind::Assign:
    return "Assign";
  case TokenKind::EqualEqual:
    return "EqualEqual";
  case TokenKind::BangEqual:
    return "BangEqual";
  case TokenKind::Less:
    return "Less";
  case TokenKind::LessEqual:
    return "LessEqual";
  case TokenKind::Greater:
    return "Greater";
  case TokenKind::GreaterEqual:
    return "GreaterEqual";
  case TokenKind::AmpAmp:
    return "AmpAmp";
  case TokenKind::PipePipe:
    return "PipePipe";
  case TokenKind::Bang:
    return "Bang";
  case TokenKind::Eof:
    return "Eof";
  case TokenKind::Error:
    return "Error";
  }
  return "Unknown";
}

} // namespace mlang
