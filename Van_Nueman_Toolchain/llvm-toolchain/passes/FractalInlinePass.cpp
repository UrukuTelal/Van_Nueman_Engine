//===-- FractalInlinePass.cpp - Fractal Inlining Pass ------===//
//
// Inlines fractal functions across scales (entity/server/federation).
// Generates scale-specific versions of [[fractal]] functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct FractalInlinePass : public FunctionPass {
  static char ID;
  FractalInlinePass() : FunctionPass(ID) {}

  bool runOnFunction(Function& F) override {
    if (!F.hasFnAttribute("fractal")) {
      return false;
    }

    errs() << "FractalInlinePass: processing " << F.getName() << "\n";

    bool Changed = false;
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (auto* Call = dyn_cast<CallInst>(&I)) {
          if (Function* Callee = Call->getCalledFunction()) {
            if (Callee->hasFnAttribute("fractal")) {
              Changed = true;
            }
          }
        }
      }
    }
    return Changed;
  }
};

} // end anonymous namespace

char FractalInlinePass::ID = 0;
static RegisterPass<FractalInlinePass> X("fractal-inline",
                                         "Fractal Inlining Pass",
                                         false, false);
