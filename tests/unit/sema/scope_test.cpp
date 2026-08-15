#include "mlang/sema/Scope.h"

#include <gtest/gtest.h>

using namespace mlang;

TEST(Scope, DefinesAndFinds) {
  Scope scope;
  scope.define("x", SemaType::Int);
  ASSERT_NE(scope.find("x"), nullptr);
  EXPECT_EQ(*scope.find("x"), SemaType::Int);
}

TEST(Scope, UnboundNameIsNull) {
  Scope scope;
  EXPECT_EQ(scope.find("missing"), nullptr);
}

TEST(Scope, RedefineInSameScopeOverwrites) {
  Scope scope;
  scope.define("x", SemaType::Int);
  scope.define("x", SemaType::Float);
  EXPECT_EQ(*scope.find("x"), SemaType::Float);
}

TEST(Scope, ChildSeesParentBinding) {
  Scope parent;
  parent.define("x", SemaType::Bool);
  Scope child(&parent);
  ASSERT_NE(child.find("x"), nullptr);
  EXPECT_EQ(*child.find("x"), SemaType::Bool);
}

TEST(Scope, ChildShadowsParentWithoutMutatingIt) {
  Scope parent;
  parent.define("x", SemaType::Bool);
  Scope child(&parent);
  child.define("x", SemaType::Int);
  EXPECT_EQ(*child.find("x"), SemaType::Int);
  EXPECT_EQ(*parent.find("x"), SemaType::Bool);
}
