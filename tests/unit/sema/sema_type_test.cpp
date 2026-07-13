#include "mlang/sema/SemaType.h"

#include <gtest/gtest.h>

using namespace mlang;

TEST(SemaType, ConvertsTypeName) {
  EXPECT_EQ(toSemaType(TypeName::Int), SemaType::Int);
  EXPECT_EQ(toSemaType(TypeName::Float), SemaType::Float);
  EXPECT_EQ(toSemaType(TypeName::Bool), SemaType::Bool);
}

TEST(SemaType, NamesEveryVariant) {
  EXPECT_EQ(semaTypeName(SemaType::Int), "int");
  EXPECT_EQ(semaTypeName(SemaType::Float), "float");
  EXPECT_EQ(semaTypeName(SemaType::Bool), "bool");
  EXPECT_EQ(semaTypeName(SemaType::String), "string");
  EXPECT_EQ(semaTypeName(SemaType::Error), "<error>");
}
