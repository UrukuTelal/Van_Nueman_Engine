#pragma once

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Types.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/ArrayRef.h"
#include "mlir/Support/TypeID.h"

namespace vnes {

// ── VNES Cell Type ───────────
class VNESCellType
    : public mlir::Type::TypeBase<VNESCellType, mlir::Type, mlir::TypeStorage> {
public:
  using Base::Base;
  static constexpr llvm::StringLiteral getMnemonic() { return "cell"; }
  static constexpr llvm::StringLiteral name = "cell";
};

// ── VNES Dialect ───────────
class VNESDialect : public mlir::Dialect {
public:
  explicit VNESDialect(mlir::MLIRContext *ctx);
  static llvm::StringRef getDialectNamespace() { return "vnes"; }

  mlir::Type parseType(mlir::DialectAsmParser &parser) const override;
  void printType(mlir::Type type,
                 mlir::DialectAsmPrinter &printer) const override;
};

// ── Ops ───────────

// Declares a 3D VNES cell lattice
class LatticeOp : public mlir::Op<LatticeOp, mlir::OpTrait::OneResult,
                                   mlir::OpTrait::ZeroOperands> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.lattice"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() { return {}; }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Type resultType);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
};

// Extracts a named field tensor from a cell lattice
class CellFieldOp : public mlir::Op<CellFieldOp, mlir::OpTrait::OneResult,
                                     mlir::OpTrait::OneOperand> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.cell_field"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() {
    static llvm::StringRef names[] = {"field"};
    return llvm::ArrayRef(names);
  }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Value lattice, llvm::StringRef fieldName);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
  llvm::StringRef getFieldName();
};

// Darcy water flux computation (2 operands: water field, conductivity)
class DarcyFluxOp
    : public mlir::Op<DarcyFluxOp, mlir::OpTrait::OneResult,
                       mlir::OpTrait::NOperands<2>::Impl> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.darcy_flux"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() {
    static llvm::StringRef names[] = {"alpha"};
    return llvm::ArrayRef(names);
  }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Value waterField, mlir::Value conductivity,
                    float alpha);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
  float getAlpha();
};

// Monod biomass growth (2 operands: biomass field, nutrient field)
class MonodGrowthOp
    : public mlir::Op<MonodGrowthOp, mlir::OpTrait::OneResult,
                       mlir::OpTrait::NOperands<2>::Impl> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.monod_growth"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() {
    static llvm::StringRef names[] = {"max_uptake", "half_saturation"};
    return llvm::ArrayRef(names);
  }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Value biomassField, mlir::Value nutrientField,
                    float maxUptake, float halfSaturation);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
  float getMaxUptake();
  float getHalfSaturation();
};

// Material decay (1 operand: field)
class DecayOp : public mlir::Op<DecayOp, mlir::OpTrait::OneResult,
                                 mlir::OpTrait::OneOperand> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.decay"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() {
    static llvm::StringRef names[] = {"rate"};
    return llvm::ArrayRef(names);
  }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Value field, float rate);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
  float getRate();
};

// Fused cell update (result of -vnes-fuse-solvers)
class FusedCellUpdateOp
    : public mlir::Op<FusedCellUpdateOp, mlir::OpTrait::OneResult,
                       mlir::OpTrait::VariadicOperands> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.fused_update"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() { return {}; }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::Type resultType, mlir::ValueRange inputs);
  mlir::LogicalResult verify();
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
};

// Terminator for fused solver region
class ReturnOp : public mlir::Op<ReturnOp, mlir::OpTrait::ZeroResults,
                                  mlir::OpTrait::VariadicOperands,
                                  mlir::OpTrait::IsTerminator> {
public:
  using Op::Op;
  static llvm::StringRef getOperationName() { return "vnes.return"; }
  static llvm::ArrayRef<llvm::StringRef> getAttributeNames() { return {}; }
  static void build(mlir::OpBuilder &builder, mlir::OperationState &state,
                    mlir::ValueRange operands);
  static mlir::ParseResult parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result);
  void print(mlir::OpAsmPrinter &p);
};

} // namespace vnes

MLIR_DECLARE_EXPLICIT_TYPE_ID(::vnes::VNESDialect)
