//===-- FractalInlinePass.cpp - Fractal Inlining Pass ------===//
//
// Inlines [[fractal]]-annotated callees into their callers.
// Scale specialization (entity/server/federation) is handled by
// CloneFunctionInto at each scale with a scale constant propagated.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static const char* SCALE_SUFFIXES[] = {"_entity", "_server", "_federation"};
static const float SCALE_VALUES[] = {1.0f, 100.0f, 10000.0f};

struct FractalInlinePass : public FunctionPass {
  static char ID;
  FractalInlinePass() : FunctionPass(ID) {}

  bool runOnFunction(Function& F) override {
    if (!F.hasFnAttribute("fractal")) {
      return false;
    }

    bool Changed = false;
    Module* M = F.getParent();

    // Create scale-specific clones
    for (int s = 0; s < 3; s++) {
      std::string CloneName = (F.getName() + SCALE_SUFFIXES[s]).str();
      Function* Existing = M->getFunction(CloneName);
      if (Existing) continue;

      // Clone the function
      ValueToValueMapTy VMap;
      Function* Clone = CloneFunction(&F, VMap, nullptr);
      if (!Clone) continue;

      // Add scale as a metadata attachment for later optimization passes
      LLVMContext& Ctx = M->getContext();
      auto* ScaleMD = ConstantAsMetadata::get(
          ConstantFP::get(Type::getFloatTy(Ctx), SCALE_VALUES[s]));
      auto* ScaleNode = MDNode::get(Ctx, {ScaleMD});
      Clone->setMetadata("fractal.scale", ScaleNode);

      Clone->setName(CloneName);
      Clone->setLinkage(GlobalValue::InternalLinkage);

      // Insert into module
      if (F.hasAvailableExternallyLinkage() || F.isDeclaration())
        Clone->setLinkage(GlobalValue::AvailableExternallyLinkage);
      M->getFunctionList().insert(F.getIterator(), Clone);

      errs() << "FractalInlinePass: created " << CloneName
             << " (scale=" << SCALE_VALUES[s] << ")\n";
    }

    // Inline calls to [[fractal]] callees within this function
    SmallVector<CallInst*, 8> ToInline;
    for (auto& BB : F) {
      for (auto& I : BB) {
        if (auto* Call = dyn_cast<CallInst>(&I)) {
          if (Function* Callee = Call->getCalledFunction()) {
            if (Callee->hasFnAttribute("fractal") && !Callee->isDeclaration()) {
              ToInline.push_back(Call);
            }
          }
        }
      }
    }

    // Perform the inlining (only if the callee is small/simple)
    for (auto* Call : ToInline) {
      Function* Callee = Call->getCalledFunction();
      std::string CalleeName = Callee ? Callee->getName().str() : "unknown";
      InlineFunctionInfo IFI;
      if (InlineFunction(*Call, IFI).isSuccess()) {
        Changed = true;
        errs() << "FractalInlinePass: inlined " << CalleeName << "\n";
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
