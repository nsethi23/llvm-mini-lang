#include "mlang/jit/DispatchTable.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}
} // namespace

TEST(DispatchTableTest, InvokeCallsInstalledTrampolineAndCountsCalls) {
  DispatchTable table;
  table.install("id", [](std::vector<Value> args, SourceLocation) { return args[0]; });

  EXPECT_TRUE(table.contains("id"));
  EXPECT_EQ(table.callCount("id"), 0u);

  Value result = table.invoke("id", {Value{int64_t{42}}}, loc());
  EXPECT_EQ(std::get<int64_t>(result), 42);
  EXPECT_EQ(table.callCount("id"), 1u);

  table.invoke("id", {Value{int64_t{7}}}, loc());
  EXPECT_EQ(table.callCount("id"), 2u);
}

TEST(DispatchTableTest, UnknownNameIsNotContainedAndHasZeroCount) {
  DispatchTable table;
  EXPECT_FALSE(table.contains("missing"));
  EXPECT_EQ(table.callCount("missing"), 0u);
}

TEST(DispatchTableTest, RedirectChangesTargetWithoutResettingCallCount) {
  DispatchTable table;
  table.install("f", [](std::vector<Value>, SourceLocation) { return Value{int64_t{1}}; });
  table.invoke("f", {}, loc());
  ASSERT_EQ(table.callCount("f"), 1u);

  table.redirect("f", [](std::vector<Value>, SourceLocation) { return Value{int64_t{2}}; });

  Value result = table.invoke("f", {}, loc());
  EXPECT_EQ(std::get<int64_t>(result), 2);
  // The counter keeps accumulating across the redirect -- it tracks total
  // calls through this dispatch entry, not calls to a specific target.
  EXPECT_EQ(table.callCount("f"), 2u);
}

TEST(DispatchTableTest, CallCountsReportsAllEntriesInInstallOrder) {
  DispatchTable table;
  table.install("a", [](std::vector<Value>, SourceLocation) { return Value{int64_t{0}}; });
  table.install("b", [](std::vector<Value>, SourceLocation) { return Value{int64_t{0}}; });
  table.invoke("b", {}, loc());
  table.invoke("b", {}, loc());
  table.invoke("a", {}, loc());

  auto counts = table.callCounts();
  ASSERT_EQ(counts.size(), 2u);
  EXPECT_EQ(counts[0].first, "a");
  EXPECT_EQ(counts[0].second, 1u);
  EXPECT_EQ(counts[1].first, "b");
  EXPECT_EQ(counts[1].second, 2u);
}
