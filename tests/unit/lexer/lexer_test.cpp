#include "mlang/lexer/Lexer.h"

#include <gtest/gtest.h>

using mlang::Lexer;
using mlang::Token;
using mlang::TokenKind;

namespace {
std::vector<Token> lex(const std::string& source) {
  return Lexer(source).tokenize();
}
} // namespace

TEST(Lexer, EmptySourceProducesOnlyEof) {
  auto tokens = lex("");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, TokenKind::Eof);
}

TEST(Lexer, SkipsWhitespaceBetweenIdentifiers) {
  auto tokens = lex("  foo \t bar\n\nbaz  ");
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].lexeme, "foo");
  EXPECT_EQ(tokens[1].lexeme, "bar");
  EXPECT_EQ(tokens[2].lexeme, "baz");
  EXPECT_EQ(tokens[3].kind, TokenKind::Eof);
}

TEST(Lexer, ScansPlainIdentifiers) {
  auto tokens = lex("foo _bar baz123 _123");
  ASSERT_EQ(tokens.size(), 5u);
  for (int i = 0; i < 4; i++)
    EXPECT_EQ(tokens[i].kind, TokenKind::Identifier);
  EXPECT_EQ(tokens[0].lexeme, "foo");
  EXPECT_EQ(tokens[1].lexeme, "_bar");
  EXPECT_EQ(tokens[2].lexeme, "baz123");
  EXPECT_EQ(tokens[3].lexeme, "_123");
}

TEST(Lexer, RecognizesAllKeywords) {
  auto tokens = lex("fn let return if else while true false as int float bool");
  std::vector<TokenKind> expected = {
      TokenKind::KwFn,   TokenKind::KwLet,   TokenKind::KwReturn, TokenKind::KwIf,
      TokenKind::KwElse, TokenKind::KwWhile, TokenKind::KwTrue,   TokenKind::KwFalse,
      TokenKind::KwAs,   TokenKind::KwInt,   TokenKind::KwFloat,  TokenKind::KwBool,
  };
  ASSERT_EQ(tokens.size(), expected.size() + 1);
  for (size_t i = 0; i < expected.size(); i++)
    EXPECT_EQ(tokens[i].kind, expected[i]) << "token " << i;
}

TEST(Lexer, KeywordPrefixedIdentifierIsStillAnIdentifier) {
  auto tokens = lex("fnord iffy elsewhere");
  ASSERT_EQ(tokens.size(), 4u);
  EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
  EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
  EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
}

TEST(Lexer, TracksLineAndColumn) {
  auto tokens = lex("foo\n  bar");
  ASSERT_EQ(tokens.size(), 3u);
  EXPECT_EQ(tokens[0].loc.line, 1);
  EXPECT_EQ(tokens[0].loc.column, 1);
  EXPECT_EQ(tokens[1].loc.line, 2);
  EXPECT_EQ(tokens[1].loc.column, 3);
}

TEST(Lexer, UnknownCharacterProducesErrorToken) {
  auto tokens = lex("@");
  ASSERT_EQ(tokens.size(), 2u);
  EXPECT_EQ(tokens[0].kind, TokenKind::Error);
}
