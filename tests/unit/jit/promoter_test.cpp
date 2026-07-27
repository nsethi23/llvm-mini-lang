#include "mlang/ast/Decl.h"
#include "mlang/jit/DispatchTable.h"
#include "mlang/jit/Promoter.h"

#include "llvm/Support/raw_ostream.h"

#include <gtest/gtest.h>

using namespace mlang;

namespace {
SourceLocation loc(int line = 1, int col = 1) {
  return {line, col};
}

std::unique_ptr<BlockStmt> blockOf(std::vector<StmtPtr> stmts) {
  return std::make_unique<BlockStmt>(std::move(stmts), loc());
}

// fn double(n: int) -> int { return n + n; }
FunctionDecl makeDoubleFn() {
  FunctionDecl fn;
  fn.name = "double";
  fn.params.push_back(Param{"n", TypeName::Int, loc()});
  fn.returnType = TypeName::Int;
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ReturnStmt>(
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("n", loc()),
                                   std::make_unique<IdentifierExpr>("n", loc()), loc()),
      loc()));
  fn.body = blockOf(std::move(body));
  return fn;
}

// Interpreter trampoline stand-in that records every call it's asked to
// make, so a test can assert it was never invoked again after promotion.
DispatchTable::Trampoline countingTrampoline(int& callCount) {
  return [&callCount](std::vector<Value> args, SourceLocation) -> Value {
    callCount++;
    return Value{std::get<int64_t>(args[0]) * 2};
  };
}
} // namespace

TEST(PromoterTest, DoesNotPromoteBelowThreshold) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  DispatchTable dispatch;
  dispatch.install("double", [](std::vector<Value> args, SourceLocation) {
    return Value{std::get<int64_t>(args[0]) * 2};
  });

  Promoter promoter(program, dispatch, /*threshold=*/3);
  promoter.attach();

  dispatch.invoke("double", {Value{int64_t{1}}}, loc());
  dispatch.invoke("double", {Value{int64_t{2}}}, loc());
  EXPECT_FALSE(promoter.isPromoted("double"));
}

TEST(PromoterTest, PromotesOnceCallCountCrossesThresholdAndRedirectsFutureCalls) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  DispatchTable dispatch;
  dispatch.install("double", [](std::vector<Value> args, SourceLocation) {
    return Value{std::get<int64_t>(args[0]) * 2};
  });

  Promoter promoter(program, dispatch, /*threshold=*/2);
  promoter.attach();

  Value first = dispatch.invoke("double", {Value{int64_t{10}}}, loc());
  EXPECT_EQ(std::get<int64_t>(first), 20);
  EXPECT_FALSE(promoter.isPromoted("double"));

  // This is the crossing call -- PRD.md M7 says it still finishes via
  // whatever trampoline was current when it started (the interpreter
  // stand-in above), and only calls made *after* it see native code.
  Value second = dispatch.invoke("double", {Value{int64_t{11}}}, loc());
  EXPECT_EQ(std::get<int64_t>(second), 22);
  EXPECT_TRUE(promoter.isPromoted("double"));

  Value third = dispatch.invoke("double", {Value{int64_t{21}}}, loc());
  EXPECT_EQ(std::get<int64_t>(third), 42);
}

TEST(PromoterTest, NeverReEntersTheInterpreterOncePromoted) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  DispatchTable dispatch;
  int interpreterCalls = 0;
  dispatch.install("double", countingTrampoline(interpreterCalls));

  Promoter promoter(program, dispatch, /*threshold=*/1);
  promoter.attach();

  // The 1st call crosses the threshold and still runs the interpreter
  // trampoline captured before promotion -- PRD.md's "in-flight frames
  // finish however they started".
  dispatch.invoke("double", {Value{int64_t{5}}}, loc());
  ASSERT_TRUE(promoter.isPromoted("double"));
  EXPECT_EQ(interpreterCalls, 1);

  // Every subsequent call must go native -- the interpreter trampoline's
  // call count must not budge.
  Value result = dispatch.invoke("double", {Value{int64_t{5}}}, loc());
  EXPECT_EQ(std::get<int64_t>(result), 10);
  EXPECT_EQ(interpreterCalls, 1);
}

TEST(PromoterTest, TracesThePromotionEventWithTheCallCountThatTriggeredIt) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  DispatchTable dispatch;
  dispatch.install("double", [](std::vector<Value> args, SourceLocation) {
    return Value{std::get<int64_t>(args[0]) * 2};
  });

  std::string traced;
  llvm::raw_string_ostream traceOs(traced);
  Promoter promoter(program, dispatch, /*threshold=*/1, &traceOs);
  promoter.attach();

  dispatch.invoke("double", {Value{int64_t{1}}}, loc());
  EXPECT_EQ(traced, "double promoted to native code after 1 calls\n");
}

TEST(PromoterTest, NeverPromotesTheSameFunctionTwice) {
  Program program;
  program.functions.push_back(makeDoubleFn());
  DispatchTable dispatch;
  dispatch.install("double", [](std::vector<Value> args, SourceLocation) {
    return Value{std::get<int64_t>(args[0]) * 2};
  });

  std::string traced;
  llvm::raw_string_ostream traceOs(traced);
  Promoter promoter(program, dispatch, /*threshold=*/1, &traceOs);
  promoter.attach();

  for (int i = 0; i < 5; i++)
    dispatch.invoke("double", {Value{int64_t{1}}}, loc());

  // Only one promotion line, no matter how many more calls cross the
  // (already-crossed) threshold afterward.
  EXPECT_EQ(traced, "double promoted to native code after 1 calls\n");
}
