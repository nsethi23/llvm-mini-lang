#include "mlang/ast/Decl.h"
#include "mlang/codegen/CodeGen.h"

#include "llvm/IR/Verifier.h"
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

void expectVerifies(llvm::Module& module) {
  std::string errors;
  llvm::raw_string_ostream os(errors);
  bool broken = llvm::verifyModule(module, &os);
  EXPECT_FALSE(broken) << errors;
}
} // namespace

TEST(CodeGenProgram, GeneratesVerifiableMainReturningALiteral) {
  // fn main() -> int { return 42; }
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteralExpr>(42, loc()), loc()));
  main.body = blockOf(std::move(body));

  Program program;
  program.functions.push_back(std::move(main));

  llvm::LLVMContext ctx;
  CodeGen cg(program, ctx);
  cg.generate();
  expectVerifies(cg.module());

  llvm::Function* mainFn = cg.module().getFunction("main");
  ASSERT_NE(mainFn, nullptr);
  EXPECT_FALSE(mainFn->isDeclaration());
}

TEST(CodeGenProgram, GeneratesVerifiableIfElseWhereBothArmsReturn) {
  // fn main() -> int { if true { return 1; } else { return 0; } }
  // Both arms return, so the if/else merge block is unreachable -- this
  // exercises the `unreachable`-closing fallback in CodeGen::genFunction.
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  std::vector<StmtPtr> thenStmts;
  thenStmts.push_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteralExpr>(1, loc()), loc()));
  std::vector<StmtPtr> elseStmts;
  elseStmts.push_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteralExpr>(0, loc()), loc()));
  std::vector<StmtPtr> body;
  body.push_back(std::make_unique<IfStmt>(std::make_unique<BoolLiteralExpr>(true, loc()),
                                          blockOf(std::move(thenStmts)),
                                          blockOf(std::move(elseStmts)), loc()));
  main.body = blockOf(std::move(body));

  Program program;
  program.functions.push_back(std::move(main));

  llvm::LLVMContext ctx;
  CodeGen cg(program, ctx);
  cg.generate();
  expectVerifies(cg.module());
}

TEST(CodeGenProgram, GeneratesVerifiableRecursiveFibWithPrint) {
  // Mirrors InterpreterRun.RunsFullFibProgramAndPrintsResult so the same
  // program is exercised by both the interpreter oracle and codegen.
  // fn fib(n: int) -> int { if n < 2 { return n; } else { return fib(n-1) + fib(n-2); } }
  // fn main() -> int { let x: int = 10; let result: int = fib(x); print(result); return 0; }
  FunctionDecl fib;
  fib.name = "fib";
  fib.params.push_back(Param{"n", TypeName::Int, loc()});
  fib.returnType = TypeName::Int;
  {
    auto cond =
        std::make_unique<BinaryExpr>(BinaryOp::Lt, std::make_unique<IdentifierExpr>("n", loc()),
                                     std::make_unique<IntLiteralExpr>(2, loc()), loc());
    std::vector<StmtPtr> thenStmts;
    thenStmts.push_back(
        std::make_unique<ReturnStmt>(std::make_unique<IdentifierExpr>("n", loc()), loc()));

    std::vector<ExprPtr> m1Args;
    m1Args.push_back(
        std::make_unique<BinaryExpr>(BinaryOp::Sub, std::make_unique<IdentifierExpr>("n", loc()),
                                     std::make_unique<IntLiteralExpr>(1, loc()), loc()));
    auto call1 = std::make_unique<CallExpr>("fib", std::move(m1Args), loc());
    std::vector<ExprPtr> m2Args;
    m2Args.push_back(
        std::make_unique<BinaryExpr>(BinaryOp::Sub, std::make_unique<IdentifierExpr>("n", loc()),
                                     std::make_unique<IntLiteralExpr>(2, loc()), loc()));
    auto call2 = std::make_unique<CallExpr>("fib", std::move(m2Args), loc());
    std::vector<StmtPtr> elseStmts;
    elseStmts.push_back(std::make_unique<ReturnStmt>(
        std::make_unique<BinaryExpr>(BinaryOp::Add, std::move(call1), std::move(call2), loc()),
        loc()));

    std::vector<StmtPtr> body;
    body.push_back(std::make_unique<IfStmt>(std::move(cond), blockOf(std::move(thenStmts)),
                                            blockOf(std::move(elseStmts)), loc()));
    fib.body = blockOf(std::move(body));
  }

  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  {
    std::vector<StmtPtr> body;
    body.push_back(std::make_unique<LetStmt>("x", TypeName::Int,
                                             std::make_unique<IntLiteralExpr>(10, loc()), loc()));
    std::vector<ExprPtr> fibArgs;
    fibArgs.push_back(std::make_unique<IdentifierExpr>("x", loc()));
    body.push_back(std::make_unique<LetStmt>(
        "result", TypeName::Int, std::make_unique<CallExpr>("fib", std::move(fibArgs), loc()),
        loc()));
    std::vector<ExprPtr> printArgs;
    printArgs.push_back(std::make_unique<IdentifierExpr>("result", loc()));
    body.push_back(std::make_unique<ExprStmt>(
        std::make_unique<CallExpr>("print", std::move(printArgs), loc()), loc()));
    body.push_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteralExpr>(0, loc()), loc()));
    main.body = blockOf(std::move(body));
  }

  Program program;
  program.functions.push_back(std::move(fib));
  program.functions.push_back(std::move(main));

  llvm::LLVMContext ctx;
  CodeGen cg(program, ctx);
  cg.generate();
  expectVerifies(cg.module());

  llvm::Function* fibFn = cg.module().getFunction("fib");
  ASSERT_NE(fibFn, nullptr);
  EXPECT_FALSE(fibFn->isDeclaration());
  EXPECT_NE(cg.module().getFunction("mlang_print_int"), nullptr);
}

TEST(CodeGenProgram, GeneratesVerifiableWhileLoop) {
  // fn main() -> int { let i: int = 0; while i < 5 { i = i + 1; } return i; }
  FunctionDecl main;
  main.name = "main";
  main.returnType = TypeName::Int;
  std::vector<StmtPtr> body;
  body.push_back(
      std::make_unique<LetStmt>("i", TypeName::Int, std::make_unique<IntLiteralExpr>(0, loc()), loc()));

  auto cond = std::make_unique<BinaryExpr>(BinaryOp::Lt, std::make_unique<IdentifierExpr>("i", loc()),
                                           std::make_unique<IntLiteralExpr>(5, loc()), loc());
  std::vector<StmtPtr> whileBody;
  whileBody.push_back(std::make_unique<AssignStmt>(
      "i",
      std::make_unique<BinaryExpr>(BinaryOp::Add, std::make_unique<IdentifierExpr>("i", loc()),
                                   std::make_unique<IntLiteralExpr>(1, loc()), loc()),
      loc()));
  body.push_back(
      std::make_unique<WhileStmt>(std::move(cond), blockOf(std::move(whileBody)), loc()));
  body.push_back(std::make_unique<ReturnStmt>(std::make_unique<IdentifierExpr>("i", loc()), loc()));
  main.body = blockOf(std::move(body));

  Program program;
  program.functions.push_back(std::move(main));

  llvm::LLVMContext ctx;
  CodeGen cg(program, ctx);
  cg.generate();
  expectVerifies(cg.module());
}
