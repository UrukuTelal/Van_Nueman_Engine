#pragma once

#include "mlir/Pass/Pass.h"
#include <memory>

namespace vnes {

/// Creates a pass that fuses DarcyFluxOp + MonodGrowthOp + DecayOp
/// sequences into single FusedCellUpdateOp operations.
std::unique_ptr<mlir::Pass> createVnesFuseSolversPass();

/// Creates a pass that lowers VNES ops to linalg.generic element-wise ops.
std::unique_ptr<mlir::Pass> createVnesLowerToLinalgPass();

/// Registers the VNES lowering pass.
void registerVNESLowerToLinalgPass();

/// Registers all VNES passes.
void registerVNESPasses();

} // namespace vnes
