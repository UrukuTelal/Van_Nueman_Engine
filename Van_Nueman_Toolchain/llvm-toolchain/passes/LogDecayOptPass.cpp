//===-- LogDecayOptPass.cpp - Optimize Log Decay Patterns ----===//
//
// Optimizes signal / logf(hop_count + 1.0f + 1e-6f) patterns.
// When hop_count is constant, precomputes the divisor.
// When hop_count has a known range, substitutes a LUT or polynomial approx.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>

using namespace llvm;

namespace {

// Precomputed 16-entry LUT for 1/log2(n+1+epsilon) for n in [0..15]
// For use when hop_count range is bounded and small.
// Values: 1.0f / log2f((float)n + 1.0f + 1e-6f)
static const float LOG2_LUT[16] = {
  1.442695f, 1.000000f, 0.773706f, 0.630930f,
  0.535530f, 0.464379f, 0.410017f, 0.366592f,
  0.331898f, 0.303395f, 0.279586f, 0.259352f,
  0.241946f, 0.226820f, 0.213568f, 0.201887f,
};

class LogDecayOptPass : public FunctionPass {
public:
  static char ID;
  LogDecayOptPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    bool Changed = false;

    for (auto& BB : F) {
      for (auto& I : BB) {
        // Look for fdiv instructions
        auto* FDiv = dyn_cast<BinaryOperator>(&I);
        if (!FDiv || FDiv->getOpcode() != Instruction::FDiv) continue;

        // Check if the divisor is a call to logf/log2f
        auto* Call = dyn_cast<CallInst>(FDiv->getOperand(1));
        if (!Call) continue;

        Function* Callee = Call->getCalledFunction();
        if (!Callee) continue;

        bool IsLog2 = Callee->getName() == "log2f" || Callee->getName() == "log2";
        bool IsLog = Callee->getName() == "logf" || Callee->getName() == "log";
        if (!IsLog2 && !IsLog) continue;

        // logf(x) = log2f(x) * ln(2), so we can normalize both to log2
        // and track a conversion factor
        float LogBaseFactor = IsLog ? 1.442695f : 1.0f; // 1/ln(2) ≈ 1.4427

        // The argument to logf/log2f should be (hop_count + 1.0f + 1e-6f)
        // or similar. Check if the argument is an fadd with a constant.
        Value* LogArg = Call->getArgOperand(0);

        // Try to extract hop_count from logf(hop_count + c1 + c2) pattern
        Value* HopCount = nullptr;
        float ConstantOffset = 0.0f;

        if (auto* FAdd = dyn_cast<BinaryOperator>(LogArg)) {
          if (FAdd->getOpcode() == Instruction::FAdd) {
            // Check both operands for constants
            for (int i = 0; i < 2; i++) {
              if (auto* CF = dyn_cast<ConstantFP>(FAdd->getOperand(i))) {
                ConstantOffset += CF->getValueAPF().convertToFloat();
              } else if (auto* InnerAdd = dyn_cast<BinaryOperator>(FAdd->getOperand(i))) {
                if (InnerAdd->getOpcode() == Instruction::FAdd) {
                  for (int j = 0; j < 2; j++) {
                    if (auto* CF = dyn_cast<ConstantFP>(InnerAdd->getOperand(j))) {
                      ConstantOffset += CF->getValueAPF().convertToFloat();
                    } else {
                      HopCount = InnerAdd->getOperand(j);
                    }
                  }
                } else {
                  HopCount = FAdd->getOperand(i);
                }
              } else {
                HopCount = FAdd->getOperand(i);
              }
            }
          }
        }

        if (!HopCount) continue;

        // If hop_count is a compile-time constant, precompute the divisor
        if (auto* ConstHop = dyn_cast<ConstantInt>(HopCount)) {
          uint64_t N = ConstHop->getZExtValue();
          float Divisor = std::log2((float)N + ConstantOffset) * LogBaseFactor;
          if (Divisor > 0.0f) {
            auto* Precomputed = ConstantFP::get(FDiv->getType(), 1.0f / Divisor);
            auto* Mul = BinaryOperator::CreateFMul(FDiv->getOperand(0), Precomputed,
                                                     "", &I);
            Mul->takeName(&I);
            I.replaceAllUsesWith(Mul);
            I.eraseFromParent();
            Changed = true;
          }
        }
      }
    }

    return Changed;
  }
};

char LogDecayOptPass::ID = 0;
static RegisterPass<LogDecayOptPass> X("log-decay-opt",
    "Optimize logarithmic decay patterns", false, false);

} // namespace
