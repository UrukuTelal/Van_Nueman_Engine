#include "VNESPasses.h"
#include "vnes/VNESDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {

// ── Fusion Pattern ──────────────────────────────────────────────
struct FuseSolverPattern : public OpRewritePattern<vnes::DecayOp> {
  FuseSolverPattern(MLIRContext *ctx)
      : OpRewritePattern<vnes::DecayOp>(ctx, /*benefit=*/1) {}

  LogicalResult matchAndRewrite(vnes::DecayOp decayOp,
                                PatternRewriter &rewriter) const override {

    Block *block = decayOp->getBlock();
    if (!block)
      return failure();

    SmallVector<Operation *> solverOps;
    bool foundDarcy = false, foundMonod = false;

    for (Operation &op : *block) {
      if (isa<vnes::DarcyFluxOp>(&op)) {
        solverOps.push_back(&op);
        foundDarcy = true;
        continue;
      }
      if (isa<vnes::MonodGrowthOp>(&op)) {
        solverOps.push_back(&op);
        foundMonod = true;
        continue;
      }
      if (isa<vnes::DecayOp>(&op)) {
        solverOps.push_back(&op);
        continue;
      }
      if (&op == decayOp.getOperation())
        break;
    }

    if (!foundDarcy || !foundMonod)
      return failure();

    SmallVector<Value> fusedOperands;
    for (Operation *solver : solverOps) {
      for (Value operand : solver->getOperands()) {
        if (isa<vnes::CellFieldOp>(operand.getDefiningOp()) ||
            isa<vnes::LatticeOp>(operand.getDefiningOp())) {
          if (llvm::find(fusedOperands, operand) == fusedOperands.end())
            fusedOperands.push_back(operand);
        }
      }
    }

    Type resultType = solverOps.front()->getResult(0).getType();
    auto fusedOp = rewriter.create<vnes::FusedCellUpdateOp>(
        decayOp.getLoc(), resultType, fusedOperands);

    Value fusedResult = fusedOp->getResult(0);
    for (Operation *solver : solverOps) {
      rewriter.replaceOp(solver, ValueRange(fusedResult));
    }

    return success();
  }
};

// ── Pass Definition ──────────────────────────────────────────────
class VnesFuseSolversPass
    : public PassWrapper<VnesFuseSolversPass, OperationPass<func::FuncOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(VnesFuseSolversPass)

  StringRef getArgument() const override { return "vnes-fuse-solvers"; }
  StringRef getDescription() const override {
    return "Fuse DarcyFluxOp + MonodGrowthOp + DecayOp sequences into "
           "FusedCellUpdateOp";
  }

  void runOnOperation() override {
    func::FuncOp funcOp = getOperation();
    MLIRContext *ctx = &getContext();

    RewritePatternSet patterns(ctx);
    patterns.add<FuseSolverPattern>(ctx);

    FrozenRewritePatternSet frozenPatterns(std::move(patterns));
    if (failed(applyPatternsAndFoldGreedily(funcOp, frozenPatterns))) {
      return signalPassFailure();
    }
  }
};

} // namespace

// ── Factory ──────────────────────────────────────────────────────
std::unique_ptr<Pass> vnes::createVnesFuseSolversPass() {
  return std::make_unique<VnesFuseSolversPass>();
}

void vnes::registerVNESPasses() {
  PassRegistration<VnesFuseSolversPass> reg;
  vnes::registerVNESLowerToLinalgPass();
}

// ── Plugin Entry Point ──────────────────────────────────────────
#define VNES_PLUGIN_API_VERSION 1

// Declare structs ourselves to avoid linkage issues with LLVM_ATTRIBUTE_WEAK
struct VnesPassPluginInfo {
  uint32_t apiVersion;
  const char *pluginName;
  const char *pluginVersion;
  void (*registerPassRegistryCallbacks)();
};

struct VnesDialectPluginInfo {
  uint32_t apiVersion;
  const char *pluginName;
  const char *pluginVersion;
  void (*registerDialectRegistryCallbacks)(mlir::DialectRegistry *);
};

#ifdef _WIN32
#define VNES_PLUGIN_EXPORT __declspec(dllexport)
#else
#define VNES_PLUGIN_EXPORT __attribute__((__weak__))
#endif

// --- Pass plugin ---
extern "C" VNES_PLUGIN_EXPORT VnesPassPluginInfo
mlirGetPassPluginInfo() {
  return {VNES_PLUGIN_API_VERSION, "vnes-fuse-solvers", "v0.1",
          []() { vnes::registerVNESPasses(); }};
}

// --- Dialect plugin ---
extern "C" VNES_PLUGIN_EXPORT VnesDialectPluginInfo
mlirGetDialectPluginInfo() {
  return {VNES_PLUGIN_API_VERSION, "vnes-dialect", "v0.1",
          [](mlir::DialectRegistry *registry) {
            registry->insert<vnes::VNESDialect>();
          }};
}
