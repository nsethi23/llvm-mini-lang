#include "mlang/lexer/TokenKind.h"

#include <gtest/gtest.h>

using mlang::TokenKind;
using mlang::tokenKindName;

TEST(TokenKind, NamesKeywordsAndOperatorsDistinctly) {
  EXPECT_EQ(tokenKindName(TokenKind::KwFn), "KwFn");
  EXPECT_EQ(tokenKindName(TokenKind::Identifier), "Identifier");
  EXPECT_EQ(tokenKindName(TokenKind::Arrow), "Arrow");
  EXPECT_EQ(tokenKindName(TokenKind::EqualEqual), "EqualEqual");
  EXPECT_EQ(tokenKindName(TokenKind::Eof), "Eof");
  EXPECT_NE(tokenKindName(TokenKind::Plus), tokenKindName(TokenKind::Minus));
}
