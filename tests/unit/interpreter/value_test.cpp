#include "mlang/interpreter/Environment.h"
#include "mlang/interpreter/Value.h"

#include <gtest/gtest.h>

using namespace mlang;

TEST(Value, RendersEachAlternative) {
  EXPECT_EQ(valueToString(Value{int64_t{42}}), "42");
  EXPECT_EQ(valueToString(Value{true}), "true");
  EXPECT_EQ(valueToString(Value{false}), "false");
  EXPECT_EQ(valueToString(Value{std::string("hi")}), "hi");
}

TEST(Value, ReportsTypeNames) {
  EXPECT_EQ(valueTypeName(Value{int64_t{1}}), "int");
  EXPECT_EQ(valueTypeName(Value{1.5}), "float");
  EXPECT_EQ(valueTypeName(Value{true}), "bool");
  EXPECT_EQ(valueTypeName(Value{std::string("s")}), "string");
}

TEST(Environment, DefineThenFind) {
  Environment env;
  env.define("x", Value{int64_t{5}});
  const Value* found = env.find("x");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(std::get<int64_t>(*found), 5);
}

TEST(Environment, FindMissingReturnsNull) {
  Environment env;
  EXPECT_EQ(env.find("missing"), nullptr);
}

TEST(Environment, FindSearchesEnclosingScopes) {
  Environment outer;
  outer.define("x", Value{int64_t{1}});
  Environment inner(&outer);
  const Value* found = inner.find("x");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(std::get<int64_t>(*found), 1);
}

TEST(Environment, InnerDefineShadowsOuter) {
  Environment outer;
  outer.define("x", Value{int64_t{1}});
  Environment inner(&outer);
  inner.define("x", Value{int64_t{2}});
  EXPECT_EQ(std::get<int64_t>(*inner.find("x")), 2);
  EXPECT_EQ(std::get<int64_t>(*outer.find("x")), 1);
}

TEST(Environment, AssignUpdatesExistingBindingInEnclosingScope) {
  Environment outer;
  outer.define("x", Value{int64_t{1}});
  Environment inner(&outer);
  EXPECT_TRUE(inner.assign("x", Value{int64_t{99}}));
  EXPECT_EQ(std::get<int64_t>(*outer.find("x")), 99);
}

TEST(Environment, AssignToUndefinedNameFails) {
  Environment env;
  EXPECT_FALSE(env.assign("missing", Value{int64_t{1}}));
}
