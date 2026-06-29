#pragma once

#include "mlir/Pass/Pass.h"
#include <memory>

namespace vnes {

/// Creates a pass that fuses DarcyFluxOp + MonodGrowthOp + DecayOp
/// sequences into single FusedCellUpdateOp operations.
std::unique_ptr<mlir::Pass> createVnesFuseSolversPass();

/// Registers the VNES passes.
void registerVNESPasses();

} // namespace vnes
