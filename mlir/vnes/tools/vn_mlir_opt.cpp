// VNES custom mlir-opt — statically links VNES dialect to avoid
// MSVC cross-DLL TypeID issues with FallbackTypeIDResolver.
#include "VNESPasses.h"
#include "vnes/VNESDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char **argv) {
  mlir::DialectRegistry registry;
  registry.insert<mlir::arith::ArithDialect>();
  registry.insert<mlir::func::FuncDialect>();
  registry.insert<vnes::VNESDialect>();
  vnes::registerVNESPasses();
  return mlir::asMainReturnCode(mlir::MlirOptMain(
      argc, argv, "VNES MLIR Optimizer", registry));
}
