#include "vnes/VNESDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"

using namespace vnes;

MLIR_DEFINE_EXPLICIT_TYPE_ID(::vnes::VNESDialect)

VNESDialect::VNESDialect(mlir::MLIRContext *ctx)
    : mlir::Dialect(getDialectNamespace(), ctx,
                    mlir::TypeID::get<VNESDialect>()) {
  addTypes<VNESCellType>();
  addOperations<LatticeOp, CellFieldOp, DarcyFluxOp, MonodGrowthOp, DecayOp,
                FusedCellUpdateOp, ReturnOp>();
}

mlir::Type VNESDialect::parseType(mlir::DialectAsmParser &parser) const {
  if (parser.parseKeyword("cell"))
    return mlir::Type();
  return VNESCellType::get(getContext());
}

void VNESDialect::printType(mlir::Type type,
                             mlir::DialectAsmPrinter &printer) const {
  if (mlir::isa<VNESCellType>(type))
    printer << "cell";
}

// ── LatticeOp ───────────
void LatticeOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                       mlir::Type resultType) {
  state.addTypes(resultType);
}

mlir::LogicalResult LatticeOp::verify() {
  auto t = (*this)->getResult(0).getType();
  if (!mlir::isa<mlir::RankedTensorType>(t))
    return emitOpError("result must be a ranked tensor of !vnes.cell");
  auto tensorT = mlir::cast<mlir::RankedTensorType>(t);
  auto elemT = tensorT.getElementType();
  if (!mlir::isa<VNESCellType>(elemT))
    return emitOpError("element type must be !vnes.cell, got ") << elemT;
  return mlir::success();
}

mlir::ParseResult LatticeOp::parse(mlir::OpAsmParser &parser,
                                    mlir::OperationState &result) {
  mlir::Type t;
  if (parser.parseType(t))
    return mlir::failure();
  result.addTypes(t);
  return mlir::success();
}

void LatticeOp::print(mlir::OpAsmPrinter &p) {
  p << ' ';
  p << (*this)->getResult(0).getType();
}

// ── CellFieldOp ───────────
void CellFieldOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                         mlir::Value lattice, llvm::StringRef fieldName) {
  state.addOperands(lattice);
  auto tensorT = mlir::cast<mlir::RankedTensorType>(lattice.getType());
  auto fieldT = mlir::RankedTensorType::get(tensorT.getShape(),
                                             builder.getF32Type());
  state.addTypes(fieldT);
  state.addAttribute("field", builder.getStringAttr(fieldName));
}

mlir::LogicalResult CellFieldOp::verify() {
  auto latticeT =
      mlir::dyn_cast<mlir::RankedTensorType>(getOperand().getType());
  if (!latticeT || !mlir::isa<VNESCellType>(latticeT.getElementType()))
    return emitOpError("operand must be a tensor<!vnes.cell>");
  return mlir::success();
}

llvm::StringRef CellFieldOp::getFieldName() {
  return (*this)->getAttrOfType<mlir::StringAttr>("field").getValue();
}

mlir::ParseResult CellFieldOp::parse(mlir::OpAsmParser &parser,
                                      mlir::OperationState &result) {
  mlir::OpAsmParser::UnresolvedOperand operand;
  std::string fieldName;
  mlir::Type latticeType, resultType;
  if (parser.parseOperand(operand) || parser.parseKeyword("field") ||
      parser.parseLParen() || parser.parseString(&fieldName) ||
      parser.parseRParen() || parser.parseColonType(latticeType) ||
      parser.parseArrow() || parser.parseType(resultType))
    return mlir::failure();
  if (parser.resolveOperand(operand, latticeType, result.operands))
    return mlir::failure();
  result.addTypes(resultType);
  result.addAttribute(
      "field", mlir::StringAttr::get(parser.getContext(), fieldName));
  return mlir::success();
}

void CellFieldOp::print(mlir::OpAsmPrinter &p) {
  p << ' ' << getOperand() << " field(\"" << getFieldName() << "\") : ";
  p << getOperand().getType() << " -> " << (*this)->getResult(0).getType();
}

// ── DarcyFluxOp ───────────
void DarcyFluxOp::build(mlir::OpBuilder &builder,
                         mlir::OperationState &state,
                         mlir::Value waterField, mlir::Value conductivity,
                         float alpha) {
  state.addOperands({waterField, conductivity});
  state.addTypes(waterField.getType());
  state.addAttribute("alpha", builder.getF32FloatAttr(alpha));
}

mlir::LogicalResult DarcyFluxOp::verify() {
  auto t = mlir::dyn_cast<mlir::RankedTensorType>(
      (*this)->getResult(0).getType());
  if (!t || !t.getElementType().isF32())
    return emitOpError("result must be tensor<f32>");
  return mlir::success();
}

float DarcyFluxOp::getAlpha() {
  return (*this)->getAttrOfType<mlir::FloatAttr>("alpha").getValueAsDouble();
}

mlir::ParseResult DarcyFluxOp::parse(mlir::OpAsmParser &parser,
                                      mlir::OperationState &result) {
  mlir::OpAsmParser::UnresolvedOperand w, c;
  mlir::Type t;
  double alpha;
  if (parser.parseOperand(w) || parser.parseComma() ||
      parser.parseOperand(c) || parser.parseKeyword("alpha") ||
      parser.parseLParen() || parser.parseFloat(alpha) ||
      parser.parseRParen() || parser.parseColonType(t))
    return mlir::failure();
  if (parser.resolveOperand(w, t, result.operands) ||
      parser.resolveOperand(c, t, result.operands))
    return mlir::failure();
  result.addTypes(t);
  result.addAttribute(
      "alpha", mlir::FloatAttr::get(
                   mlir::cast<mlir::RankedTensorType>(t).getElementType(),
                   static_cast<float>(alpha)));
  return mlir::success();
}

void DarcyFluxOp::print(mlir::OpAsmPrinter &p) {
  p << ' ' << getOperand(0) << ", " << getOperand(1)
    << " alpha(" << getAlpha() << ") : " << (*this)->getResult(0).getType();
}

// ── MonodGrowthOp ───────────
void MonodGrowthOp::build(mlir::OpBuilder &builder,
                           mlir::OperationState &state,
                           mlir::Value biomassField,
                           mlir::Value nutrientField, float maxUptake,
                           float halfSaturation) {
  state.addOperands({biomassField, nutrientField});
  state.addTypes(biomassField.getType());
  state.addAttribute("max_uptake", builder.getF32FloatAttr(maxUptake));
  state.addAttribute("half_saturation",
                      builder.getF32FloatAttr(halfSaturation));
}

mlir::LogicalResult MonodGrowthOp::verify() { return mlir::success(); }

float MonodGrowthOp::getMaxUptake() {
  return (*this)
      ->getAttrOfType<mlir::FloatAttr>("max_uptake")
      .getValueAsDouble();
}

float MonodGrowthOp::getHalfSaturation() {
  return (*this)
      ->getAttrOfType<mlir::FloatAttr>("half_saturation")
      .getValueAsDouble();
}

mlir::ParseResult MonodGrowthOp::parse(mlir::OpAsmParser &parser,
                                        mlir::OperationState &result) {
  mlir::OpAsmParser::UnresolvedOperand b, n;
  mlir::Type t;
  double uptake, half;
  if (parser.parseOperand(b) || parser.parseComma() ||
      parser.parseOperand(n) || parser.parseKeyword("uptake") ||
      parser.parseLParen() || parser.parseFloat(uptake) ||
      parser.parseRParen() || parser.parseKeyword("half") ||
      parser.parseLParen() || parser.parseFloat(half) ||
      parser.parseRParen() || parser.parseColonType(t))
    return mlir::failure();
  if (parser.resolveOperand(b, t, result.operands) ||
      parser.resolveOperand(n, t, result.operands))
    return mlir::failure();
  result.addTypes(t);
  auto elemType = mlir::cast<mlir::RankedTensorType>(t).getElementType();
  result.addAttribute("max_uptake",
                       mlir::FloatAttr::get(elemType, static_cast<float>(uptake)));
  result.addAttribute("half_saturation",
                       mlir::FloatAttr::get(elemType, static_cast<float>(half)));
  return mlir::success();
}

void MonodGrowthOp::print(mlir::OpAsmPrinter &p) {
  p << ' ' << getOperand(0) << ", " << getOperand(1)
    << " uptake(" << getMaxUptake() << ") half(" << getHalfSaturation()
    << ") : " << (*this)->getResult(0).getType();
}

// ── DecayOp ───────────
void DecayOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                     mlir::Value field, float rate) {
  state.addOperands(field);
  state.addTypes(field.getType());
  state.addAttribute("rate", builder.getF32FloatAttr(rate));
}

mlir::LogicalResult DecayOp::verify() { return mlir::success(); }

float DecayOp::getRate() {
  return (*this)->getAttrOfType<mlir::FloatAttr>("rate").getValueAsDouble();
}

mlir::ParseResult DecayOp::parse(mlir::OpAsmParser &parser,
                                  mlir::OperationState &result) {
  mlir::OpAsmParser::UnresolvedOperand op;
  mlir::Type t;
  double rate;
  if (parser.parseOperand(op) || parser.parseKeyword("rate") ||
      parser.parseLParen() || parser.parseFloat(rate) ||
      parser.parseRParen() || parser.parseColonType(t))
    return mlir::failure();
  if (parser.resolveOperand(op, t, result.operands))
    return mlir::failure();
  result.addTypes(t);
  auto elemType = mlir::cast<mlir::RankedTensorType>(t).getElementType();
  result.addAttribute("rate",
                       mlir::FloatAttr::get(elemType, static_cast<float>(rate)));
  return mlir::success();
}

void DecayOp::print(mlir::OpAsmPrinter &p) {
  p << ' ' << getOperand() << " rate(" << getRate()
    << ") : " << (*this)->getResult(0).getType();
}

// ── FusedCellUpdateOp ───────────
void FusedCellUpdateOp::build(mlir::OpBuilder &builder,
                               mlir::OperationState &state,
                               mlir::Type resultType,
                               mlir::ValueRange inputs) {
  state.addOperands(inputs);
  state.addTypes(resultType);
}

mlir::LogicalResult FusedCellUpdateOp::verify() { return mlir::success(); }

mlir::ParseResult FusedCellUpdateOp::parse(mlir::OpAsmParser &parser,
                                            mlir::OperationState &result) {
  llvm::SmallVector<mlir::OpAsmParser::UnresolvedOperand> operands;
  mlir::Type resultType;
  if (parser.parseOperandList(operands) ||
      parser.parseColonType(resultType))
    return mlir::failure();
  for (auto &op : operands) {
    if (parser.resolveOperand(op, resultType, result.operands))
      return mlir::failure();
  }
  result.addTypes(resultType);
  return mlir::success();
}

void FusedCellUpdateOp::print(mlir::OpAsmPrinter &p) {
  p << ' ';
  p << getOperands() << " : " << (*this)->getResult(0).getType();
}

// ── ReturnOp ───────────
void ReturnOp::build(mlir::OpBuilder &builder, mlir::OperationState &state,
                      mlir::ValueRange operands) {
  state.addOperands(operands);
}

mlir::ParseResult ReturnOp::parse(mlir::OpAsmParser &parser,
                                   mlir::OperationState &result) {
  llvm::SmallVector<mlir::OpAsmParser::UnresolvedOperand> operands;
  llvm::SmallVector<mlir::Type> types;
  if (parser.parseOperandList(operands))
    return mlir::success();
  if (!operands.empty()) {
    if (parser.parseColonTypeList(types))
      return mlir::failure();
    for (size_t i = 0; i < operands.size(); i++) {
      if (parser.resolveOperand(
              operands[i], i < types.size() ? types[i] : types.back(),
              result.operands))
        return mlir::failure();
    }
  }
  return mlir::success();
}

void ReturnOp::print(mlir::OpAsmPrinter &p) {
  if (getNumOperands() > 0) {
    p << ' ' << getOperands();
    p << " : ";
    llvm::interleaveComma(getOperandTypes(), p);
  }
}
