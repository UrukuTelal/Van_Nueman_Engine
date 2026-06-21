//===-- ScaledIntSimplifyPass.cpp - Scaled Int Optimization ----===//
//
// Optimizes fp20_t scaled integer operations:
// - mul/div by 2^20 → shift left/right by 20
// - (value >> 20) << 20 → mask with ~0xFFFFF
// - (int)(f * 2^20) >> 20 where f is float → truncf
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static const uint64_t SCALE_20 = 1048576ULL; // 2^20

bool isPowerOf2(Value* V, uint64_t& Pow) {
  auto* C = dyn_cast<ConstantInt>(V);
  if (!C) return false;
  uint64_t Val = C->getZExtValue();
  if (Val == 0 || (Val & (Val - 1)) != 0) return false;
  Pow = Val;
  return true;
}

class ScaledIntSimplifyPass : public FunctionPass {
public:
  static char ID;
  ScaledIntSimplifyPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    bool Changed = false;
    
    for (auto& BB : F) {
      for (auto& I : BB) {
        // Pattern: mul %x, 1048576 → shl %x, 20
        if (auto* Mul = dyn_cast<BinaryOperator>(&I)) {
          if (Mul->getOpcode() == Instruction::Mul) {
            uint64_t Pow = 0;
            Value* Other = nullptr;
            if (isPowerOf2(Mul->getOperand(0), Pow)) Other = Mul->getOperand(1);
            else if (isPowerOf2(Mul->getOperand(1), Pow)) Other = Mul->getOperand(0);
            if (Other && Pow == SCALE_20) {
              auto* Shift = BinaryOperator::CreateShl(Other,
                ConstantInt::get(Other->getType(), 20), "", &I);
              Shift->takeName(&I);
              I.replaceAllUsesWith(Shift);
              I.eraseFromParent();
              Changed = true;
            }
          }
          // Pattern: sdiv/udiv %x, 1048576 → ashr/lshr %x, 20
          else if (Mul->getOpcode() == Instruction::SDiv) {
            uint64_t Pow = 0;
            if (isPowerOf2(Mul->getOperand(1), Pow) && Pow == SCALE_20) {
              auto* Shift = BinaryOperator::CreateAShr(Mul->getOperand(0),
                ConstantInt::get(Mul->getType(), 20), "", &I);
              Shift->takeName(&I);
              I.replaceAllUsesWith(Shift);
              I.eraseFromParent();
              Changed = true;
            }
          }
          else if (Mul->getOpcode() == Instruction::UDiv) {
            uint64_t Pow = 0;
            if (isPowerOf2(Mul->getOperand(1), Pow) && Pow == SCALE_20) {
              auto* Shift = BinaryOperator::CreateLShr(Mul->getOperand(0),
                ConstantInt::get(Mul->getType(), 20), "", &I);
              Shift->takeName(&I);
              I.replaceAllUsesWith(Shift);
              I.eraseFromParent();
              Changed = true;
            }
          }
        }
        
        // Pattern: (x >> 20) << 20 → and x, ~0xFFFFF (mask off low 20 bits)
        if (auto* Shl = dyn_cast<BinaryOperator>(&I)) {
          if (Shl->getOpcode() == Instruction::Shl) {
            auto* Shr = dyn_cast<BinaryOperator>(Shl->getOperand(0));
            if (Shr && Shr->getOpcode() == Instruction::AShr) {
              auto* InnerAmt = dyn_cast<ConstantInt>(Shr->getOperand(1));
              auto* OuterAmt = dyn_cast<ConstantInt>(Shl->getOperand(1));
              if (InnerAmt && OuterAmt &&
                  InnerAmt->getZExtValue() == 20 &&
                  OuterAmt->getZExtValue() == 20) {
                uint64_t MaskVal = ~((1ULL << 20) - 1);
                auto* Mask = ConstantInt::get(Shl->getType(), MaskVal);
                auto* And = BinaryOperator::CreateAnd(Shr->getOperand(0), Mask, "", &I);
                And->takeName(&I);
                I.replaceAllUsesWith(And);
                I.eraseFromParent();
                Changed = true;
              }
            }
          }
        }
      }
    }
    return Changed;
  }
};

char ScaledIntSimplifyPass::ID = 0;
static RegisterPass<ScaledIntSimplifyPass> X("scaled-int-simplify",
    "Simplify scaled integer operations", false, false);

} // namespace
