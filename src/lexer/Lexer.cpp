#include "mlang/lexer/Lexer.h"

#include <cctype>
#include <unordered_map>

namespace mlang {

namespace {
const std::unordered_map<std::string, TokenKind>& keywords() {
  static const std::unordered_map<std::string, TokenKind> kKeywords = {
      {"fn", TokenKind::KwFn},     {"let", TokenKind::KwLet},     {"return", TokenKind::KwReturn},
      {"if", TokenKind::KwIf},     {"else", TokenKind::KwElse},   {"while", TokenKind::KwWhile},
      {"true", TokenKind::KwTrue}, {"false", TokenKind::KwFalse}, {"as", TokenKind::KwAs},
      {"int", TokenKind::KwInt},   {"float", TokenKind::KwFloat}, {"bool", TokenKind::KwBool},
  };
  return kKeywords;
}
} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::isAtEnd() const {
  return pos_ >= source_.size();
}

char Lexer::peek() const {
  return isAtEnd() ? '\0' : source_[pos_];
}

char Lexer::peekNext() const {
  return pos_ + 1 >= source_.size() ? '\0' : source_[pos_ + 1];
}

char Lexer::advance() {
  char c = source_[pos_++];
  if (c == '\n') {
    line_++;
    column_ = 1;
  } else {
    column_++;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (isAtEnd() || source_[pos_] != expected)
    return false;
  advance();
  return true;
}

SourceLocation Lexer::currentLoc() const {
  return {line_, column_};
}

Token Lexer::makeToken(TokenKind kind, const std::string& lexeme, SourceLocation loc) {
  return Token{kind, lexeme, loc};
}

Token Lexer::errorToken(const std::string& message, SourceLocation loc) {
  return Token{TokenKind::Error, message, loc};
}

void Lexer::skipWhitespaceAndComments() {
  while (!isAtEnd()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance();
    } else if (c == '/' && peekNext() == '/') {
      while (!isAtEnd() && peek() != '\n')
        advance();
    } else {
      break;
    }
  }
}

Token Lexer::scanIdentifierOrKeyword() {
  SourceLocation loc = currentLoc();
  std::string text;
  while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
    text += advance();

  auto it = keywords().find(text);
  TokenKind kind = it != keywords().end() ? it->second : TokenKind::Identifier;
  return makeToken(kind, text, loc);
}

Token Lexer::scanNumber() {
  SourceLocation loc = currentLoc();
  std::string text;
  while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
    text += advance();

  bool isFloat = false;
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
    isFloat = true;
    text += advance(); // consume '.'
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
      text += advance();
  }

  return makeToken(isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, text, loc);
}

Token Lexer::scanString() {
  SourceLocation loc = currentLoc();
  advance(); // consume opening '"'

  std::string value;
  while (!isAtEnd() && peek() != '"') {
    char c = advance();
    if (c == '\n') {
      return errorToken("unterminated string literal", loc);
    }
    if (c == '\\') {
      if (isAtEnd())
        break;
      char esc = advance();
      switch (esc) {
      case 'n':
        value += '\n';
        break;
      case 't':
        value += '\t';
        break;
      case '"':
        value += '"';
        break;
      case '\\':
        value += '\\';
        break;
      default:
        return errorToken(std::string("unknown escape sequence '\\") + esc + "'", loc);
      }
      continue;
    }
    value += c;
  }

  if (isAtEnd())
    return errorToken("unterminated string literal", loc);

  advance(); // consume closing '"'
  return makeToken(TokenKind::StringLiteral, value, loc);
}

Token Lexer::nextToken() {
  skipWhitespaceAndComments();
  SourceLocation loc = currentLoc();

  if (isAtEnd())
    return makeToken(TokenKind::Eof, "", loc);

  char c = peek();
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
    return scanIdentifierOrKeyword();
  if (std::isdigit(static_cast<unsigned char>(c)))
    return scanNumber();
  if (c == '"')
    return scanString();

  advance();
  switch (c) {
  case '(':
    return makeToken(TokenKind::LParen, "(", loc);
  case ')':
    return makeToken(TokenKind::RParen, ")", loc);
  case '{':
    return makeToken(TokenKind::LBrace, "{", loc);
  case '}':
    return makeToken(TokenKind::RBrace, "}", loc);
  case ',':
    return makeToken(TokenKind::Comma, ",", loc);
  case ':':
    return makeToken(TokenKind::Colon, ":", loc);
  case ';':
    return makeToken(TokenKind::Semicolon, ";", loc);
  case '+':
    return makeToken(TokenKind::Plus, "+", loc);
  case '*':
    return makeToken(TokenKind::Star, "*", loc);
  case '/':
    return makeToken(TokenKind::Slash, "/", loc);
  case '%':
    return makeToken(TokenKind::Percent, "%", loc);
  case '-':
    if (match('>'))
      return makeToken(TokenKind::Arrow, "->", loc);
    return makeToken(TokenKind::Minus, "-", loc);
  case '=':
    if (match('='))
      return makeToken(TokenKind::EqualEqual, "==", loc);
    return makeToken(TokenKind::Assign, "=", loc);
  case '!':
    if (match('='))
      return makeToken(TokenKind::BangEqual, "!=", loc);
    return makeToken(TokenKind::Bang, "!", loc);
  case '<':
    if (match('='))
      return makeToken(TokenKind::LessEqual, "<=", loc);
    return makeToken(TokenKind::Less, "<", loc);
  case '>':
    if (match('='))
      return makeToken(TokenKind::GreaterEqual, ">=", loc);
    return makeToken(TokenKind::Greater, ">", loc);
  case '&':
    if (match('&'))
      return makeToken(TokenKind::AmpAmp, "&&", loc);
    return errorToken("unexpected character '&' (did you mean '&&'?)", loc);
  case '|':
    if (match('|'))
      return makeToken(TokenKind::PipePipe, "||", loc);
    return errorToken("unexpected character '|' (did you mean '||'?)", loc);
  default:
    return errorToken(std::string("unexpected character '") + c + "'", loc);
  }
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  Token tok;
  do {
    tok = nextToken();
    tokens.push_back(tok);
  } while (tok.kind != TokenKind::Eof);
  return tokens;
}

} // namespace mlang
