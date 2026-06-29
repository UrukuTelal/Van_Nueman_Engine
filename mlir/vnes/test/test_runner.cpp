// VNES MLIR dialect test runner — standalone executable
#include "VNESPasses.h"
#include "vnes/VNESDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace mlir;

static int testsPassed = 0;
static int testsFailed = 0;

static void testResult(bool ok, const char *name) {
  if (ok) {
    llvm::outs() << "  PASS: " << name << "\n";
    testsPassed++;
  } else {
    llvm::outs() << "  FAIL: " << name << "\n";
    testsFailed++;
  }
}

static MLIRContext makeContext() {
  DialectRegistry registry;
  registry.insert<vnes::VNESDialect>();
  registry.insert<func::FuncDialect>();
  registry.insert<arith::ArithDialect>();
  return MLIRContext(registry);
}

// ── Test 1: Dialect Registration ──────────────────────────────
static void testDialectRegistration() {
  MLIRContext ctx = makeContext();
  ctx.getOrLoadDialect<vnes::VNESDialect>();
  ctx.getOrLoadDialect<func::FuncDialect>();
  ctx.getOrLoadDialect<arith::ArithDialect>();
  bool loaded = ctx.getLoadedDialect<vnes::VNESDialect>() != nullptr;
  testResult(loaded, "dialect registration");
}

// ── Test 2: Parse and verify VNES IR ──────────────────────────
static void testParseAndVerify() {
  MLIRContext ctx = makeContext();

  auto module = parseSourceString<ModuleOp>(
      R"(
func.func @test_basic() -> tensor<4x4xf32> {
    %lattice = vnes.lattice tensor<4x4x!vnes.cell>
    %water = vnes.cell_field %lattice field("water") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %cond = vnes.cell_field %lattice field("cond") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %flux = vnes.darcy_flux %water, %cond alpha(0.001) : tensor<4x4xf32>
    return %flux : tensor<4x4xf32>
}
)",
      ParserConfig(&ctx));

  bool parsed = module.get() != nullptr;
  if (!parsed) {
    testResult(false, "parse VNES IR");
    return;
  }
  bool verified = succeeded(verify(*module));
  testResult(verified, "verify VNES IR");
}

// ── Test 3: Fusion pass ───────────────────────────────────────
static void testFusionPass() {
  MLIRContext ctx = makeContext();

  auto module = parseSourceString<ModuleOp>(
      R"(
func.func @test_fusion() -> tensor<4x4xf32> {
    %lattice = vnes.lattice tensor<4x4x!vnes.cell>
    %water = vnes.cell_field %lattice field("water") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %cond = vnes.cell_field %lattice field("cond") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %bio = vnes.cell_field %lattice field("biomass") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %nut = vnes.cell_field %lattice field("nutrient") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %det = vnes.cell_field %lattice field("detritus") : tensor<4x4x!vnes.cell> -> tensor<4x4xf32>
    %flux = vnes.darcy_flux %water, %cond alpha(0.01) : tensor<4x4xf32>
    %growth = vnes.monod_growth %bio, %nut uptake(0.5) half(0.1) : tensor<4x4xf32>
    %decay = vnes.decay %det rate(0.001) : tensor<4x4xf32>
    %0 = arith.addf %flux, %growth : tensor<4x4xf32>
    %1 = arith.subf %0, %decay : tensor<4x4xf32>
    return %1 : tensor<4x4xf32>
}
)",
      ParserConfig(&ctx));

  if (!module.get()) {
    testResult(false, "fusion parse");
    return;
  }

  PassManager pm(&ctx);
  pm.nest<func::FuncOp>().addPass(vnes::createVnesFuseSolversPass());
  bool passRan = succeeded(pm.run(*module));
  testResult(passRan, "fusion pass ran successfully");

  bool hasFusedOp = false;
  module->walk([&](vnes::FusedCellUpdateOp op) { hasFusedOp = true; });
  testResult(hasFusedOp, "fusion created FusedCellUpdateOp");
}

// ── Test 4: VNES type parsing ─────────────────────────────────
static void testTypeParsing() {
  MLIRContext ctx = makeContext();

  auto module = parseSourceString<ModuleOp>(
      R"(
func.func @test_type() {
    %l = "vnes.lattice"() : () -> tensor<2x2x!vnes.cell>
    return
}
)",
      ParserConfig(&ctx));

  bool parsed = module.get() != nullptr;
  testResult(parsed, "parse !vnes.cell type");
  if (parsed) {
    bool verified = succeeded(verify(*module));
    testResult(verified, "verify !vnes.cell type");
  }
}

// ── Test 5: Round-trip all op types ───────────────────────────
static void testAllOpsRoundTrip() {
  MLIRContext ctx = makeContext();

  auto module = parseSourceString<ModuleOp>(
      R"(
func.func @test_all_ops(%water: tensor<2x2xf32>, %cond: tensor<2x2xf32>,
                         %bio: tensor<2x2xf32>, %nut: tensor<2x2xf32>,
                         %det: tensor<2x2xf32>) -> tensor<2x2xf32> {
    %f = vnes.darcy_flux %water, %cond alpha(0.01) : tensor<2x2xf32>
    %g = vnes.monod_growth %bio, %nut uptake(0.5) half(0.1) : tensor<2x2xf32>
    %d = vnes.decay %det rate(0.001) : tensor<2x2xf32>
    %0 = arith.addf %f, %g : tensor<2x2xf32>
    %1 = arith.subf %0, %d : tensor<2x2xf32>
    return %1 : tensor<2x2xf32>
}
)",
      ParserConfig(&ctx));

  if (!module.get()) {
    testResult(false, "all ops parse");
    return;
  }
  bool verified = succeeded(verify(*module));
  testResult(verified, "all ops verify");

  std::string printed;
  llvm::raw_string_ostream sos(printed);
  module->print(sos);
  sos.flush();

  auto module2 =
      parseSourceString<ModuleOp>(printed, ParserConfig(&ctx));
  testResult(module2.get() != nullptr, "all ops round-trip");
}

int main() {
  llvm::outs() << "VNES MLIR Dialect Tests\n";
  llvm::outs() << "========================\n\n";

  testDialectRegistration();
  testParseAndVerify();
  testFusionPass();
  testTypeParsing();
  testAllOpsRoundTrip();

  llvm::outs() << "\nResults: " << testsPassed << " passed, "
               << testsFailed << " failed\n";

  return testsFailed > 0 ? 1 : 0;
}
