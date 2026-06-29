// VNES → Linalg lowering pass: converts VNES ops to element-wise linalg.generic
#include "VNESPasses.h"
#include "vnes/VNESDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LogicalResult.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace {

// ── Lower DarcyFluxOp ────────────────────────────────────────────
struct LowerDarcyFlux : public OpRewritePattern<vnes::DarcyFluxOp> {
  LowerDarcyFlux(MLIRContext *ctx)
      : OpRewritePattern<vnes::DarcyFluxOp>(ctx, /*benefit=*/1) {}

  LogicalResult matchAndRewrite(vnes::DarcyFluxOp op,
                                PatternRewriter &rewriter) const override {
    auto tensorT = dyn_cast<RankedTensorType>(op->getResult(0).getType());
    if (!tensorT) return failure();

    Location loc = op.getLoc();
    Value alpha = rewriter.create<arith::ConstantOp>(
        loc, rewriter.getF32FloatAttr(op.getAlpha()));

    auto identityMap = rewriter.getMultiDimIdentityMap(tensorT.getRank());
    SmallVector<AffineMap> maps(3, identityMap);
    auto empty = rewriter.create<tensor::EmptyOp>(loc, tensorT.getShape(),
                                                   tensorT.getElementType());

    Value result = rewriter.create<linalg::GenericOp>(
        loc, tensorT,
        /*inputs=*/ValueRange{op->getOperand(0), op->getOperand(1)},
        /*outputs=*/ValueRange{empty},
        /*indexingMaps=*/maps,
        /*iteratorTypes=*/SmallVector<utils::IteratorType>(
            static_cast<unsigned>(tensorT.getRank()),
            utils::IteratorType::parallel),
        [&](OpBuilder &b, Location l, ValueRange args) {
          Value w = args[0], c = args[1];
          Value tmp = b.create<arith::MulFOp>(l, alpha, c);
          Value out = b.create<arith::MulFOp>(l, w, tmp);
          b.create<linalg::YieldOp>(l, out);
        }).getResult(0);

    rewriter.replaceOp(op, result);
    return success();
  }
};

// ── Lower MonodGrowthOp ──────────────────────────────────────────
struct LowerMonodGrowth : public OpRewritePattern<vnes::MonodGrowthOp> {
  LowerMonodGrowth(MLIRContext *ctx)
      : OpRewritePattern<vnes::MonodGrowthOp>(ctx, /*benefit=*/1) {}

  LogicalResult matchAndRewrite(vnes::MonodGrowthOp op,
                                PatternRewriter &rewriter) const override {
    auto tensorT = dyn_cast<RankedTensorType>(op->getResult(0).getType());
    if (!tensorT) return failure();

    Location loc = op.getLoc();
    Value uptake = rewriter.create<arith::ConstantOp>(
        loc, rewriter.getF32FloatAttr(op.getMaxUptake()));
    Value half = rewriter.create<arith::ConstantOp>(
        loc, rewriter.getF32FloatAttr(op.getHalfSaturation()));

    auto identityMap = rewriter.getMultiDimIdentityMap(tensorT.getRank());
    SmallVector<AffineMap> maps(3, identityMap);
    auto empty = rewriter.create<tensor::EmptyOp>(loc, tensorT.getShape(),
                                                   tensorT.getElementType());

    Value result = rewriter.create<linalg::GenericOp>(
        loc, tensorT,
        /*inputs=*/ValueRange{op->getOperand(0), op->getOperand(1)},
        /*outputs=*/ValueRange{empty},
        /*indexingMaps=*/maps,
        /*iteratorTypes=*/SmallVector<utils::IteratorType>(
            static_cast<unsigned>(tensorT.getRank()),
            utils::IteratorType::parallel),
        [&](OpBuilder &b, Location l, ValueRange args) {
          Value bio = args[0], nut = args[1];
          Value tmp1 = b.create<arith::MulFOp>(l, uptake, bio);
          Value tmp2 = b.create<arith::MulFOp>(l, tmp1, nut);
          Value halfNut = b.create<arith::AddFOp>(l, half, nut);
          Value out = b.create<arith::DivFOp>(l, tmp2, halfNut);
          b.create<linalg::YieldOp>(l, out);
        }).getResult(0);

    rewriter.replaceOp(op, result);
    return success();
  }
};

// ── Lower DecayOp ────────────────────────────────────────────────
struct LowerDecay : public OpRewritePattern<vnes::DecayOp> {
  LowerDecay(MLIRContext *ctx)
      : OpRewritePattern<vnes::DecayOp>(ctx, /*benefit=*/1) {}

  LogicalResult matchAndRewrite(vnes::DecayOp op,
                                PatternRewriter &rewriter) const override {
    auto tensorT = dyn_cast<RankedTensorType>(op->getResult(0).getType());
    if (!tensorT) return failure();

    Location loc = op.getLoc();
    Value rate = rewriter.create<arith::ConstantOp>(
        loc, rewriter.getF32FloatAttr(op.getRate()));

    auto identityMap = rewriter.getMultiDimIdentityMap(tensorT.getRank());
    SmallVector<AffineMap> maps(2, identityMap);
    auto empty = rewriter.create<tensor::EmptyOp>(loc, tensorT.getShape(),
                                                   tensorT.getElementType());

    Value result = rewriter.create<linalg::GenericOp>(
        loc, tensorT,
        /*inputs=*/ValueRange{op->getOperand(0)},
        /*outputs=*/ValueRange{empty},
        /*indexingMaps=*/maps,
        /*iteratorTypes=*/SmallVector<utils::IteratorType>(
            static_cast<unsigned>(tensorT.getRank()),
            utils::IteratorType::parallel),
        [&](OpBuilder &b, Location l, ValueRange args) {
          Value det = args[0];
          Value out = b.create<arith::MulFOp>(l, rate, det);
          b.create<linalg::YieldOp>(l, out);
        }).getResult(0);

    rewriter.replaceOp(op, result);
    return success();
  }
};

// ── Lower FusedCellUpdateOp ──────────────────────────────────────
struct LowerFusedUpdate : public OpRewritePattern<vnes::FusedCellUpdateOp> {
  LowerFusedUpdate(MLIRContext *ctx)
      : OpRewritePattern<vnes::FusedCellUpdateOp>(ctx, /*benefit=*/1) {}

  LogicalResult matchAndRewrite(vnes::FusedCellUpdateOp op,
                                PatternRewriter &rewriter) const override {
    auto tensorT = dyn_cast<RankedTensorType>(op->getResult(0).getType());
    if (!tensorT) return failure();

    Location loc = op.getLoc();
    size_t nInputs = op->getNumOperands();
    if (nInputs == 0) return failure();

    if (nInputs == 1) {
      rewriter.replaceOp(op, op->getOperand(0));
      return success();
    }

    auto identityMap = rewriter.getMultiDimIdentityMap(tensorT.getRank());
    SmallVector<AffineMap> maps(nInputs + 1, identityMap);
    auto empty = rewriter.create<tensor::EmptyOp>(loc, tensorT.getShape(),
                                                   tensorT.getElementType());

    Value result = rewriter.create<linalg::GenericOp>(
        loc, tensorT,
        /*inputs=*/op->getOperands(),
        /*outputs=*/ValueRange{empty},
        /*indexingMaps=*/maps,
        /*iteratorTypes=*/SmallVector<utils::IteratorType>(
            static_cast<unsigned>(tensorT.getRank()),
            utils::IteratorType::parallel),
        [&](OpBuilder &b, Location l, ValueRange args) {
          Value sum = args[0];
          for (size_t i = 1; i < nInputs; i++)
            sum = b.create<arith::AddFOp>(l, sum, args[i]);
          b.create<linalg::YieldOp>(l, sum);
        }).getResult(0);

    rewriter.replaceOp(op, result);
    return success();
  }
};

} // anonymous namespace

// ── Pass Definition ──────────────────────────────────────────────
// NOTE: Outside anonymous namespace. MSVC 14.44 has a
// unique_ptr<Base> constructor overload resolution bug when the
// derived class is inside an anonymous namespace.
class VNESLowerToLinalgPass
    : public PassWrapper<VNESLowerToLinalgPass, OperationPass<func::FuncOp>> {
public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(VNESLowerToLinalgPass)

  StringRef getArgument() const override { return "vnes-lower-to-linalg"; }
  StringRef getDescription() const override {
    return "Lower VNES ops (darcy_flux, monod_growth, decay) to linalg.generic";
  }

  void getDependentDialects(mlir::DialectRegistry &registry) const override {
    registry.insert<mlir::tensor::TensorDialect,
                    mlir::linalg::LinalgDialect>();
  }

  void runOnOperation() override {
    func::FuncOp funcOp = getOperation();
    MLIRContext *ctx = &getContext();

    RewritePatternSet patterns(ctx);
    patterns.add<LowerDarcyFlux, LowerMonodGrowth, LowerDecay, LowerFusedUpdate>(
        ctx);

    FrozenRewritePatternSet frozenPatterns(std::move(patterns));
    if (failed(applyPatternsAndFoldGreedily(funcOp, frozenPatterns)))
      return signalPassFailure();
  }
};

std::unique_ptr<Pass> vnes::createVnesLowerToLinalgPass() {
  return std::unique_ptr<Pass>(new VNESLowerToLinalgPass());
}

void vnes::registerVNESLowerToLinalgPass() {
  PassRegistration<VNESLowerToLinalgPass> reg;
}
