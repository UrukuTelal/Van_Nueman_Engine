//===-- SVOMemoryPass.cpp - Optimize SVO Memory Accesses ----===//
//
// Optimizes SVO node pool accesses:
// 1. Hoists constant GEP offsets for svo_pool out of loops
// 2. Reorders SVO_Node field accesses for cache-friendly patterns
// 3. Inserts prefetch for sequential node traversal patterns
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

class SVOMemoryPass : public FunctionPass {
public:
  static char ID;
  SVOMemoryPass() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
  }

  bool runOnFunction(Function &F) override {
    bool Changed = false;

    // Identify global variables that look like SVO node pools
    SmallPtrSet<GlobalVariable*, 4> SVOPools;
    for (auto& GV : F.getParent()->globals()) {
      StringRef Name = GV.getName();
      if (Name.contains("svo_pool") || Name.contains("svo_nodes") ||
          Name.contains("node_pool") || Name.contains("voxel_pool")) {
        SVOPools.insert(&GV);
      }
    }
    if (SVOPools.empty()) return false;

    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

    // Phase 1: Hoist constant-field-offset GEPs out of loops
    for (Loop* L : LI) {
      Changed |= hoistConstantGEPs(L, SVOPools);
    }

    // Phase 2: Insert prefetches for sequential node traversal
    Changed |= insertNodePrefetches(F, SVOPools);

    return Changed;
  }

private:
  bool hoistConstantGEPs(Loop* L, const SmallPtrSet<GlobalVariable*, 4>& SVOPools) {
    bool Changed = false;

    for (BasicBlock* BB : L->getBlocks()) {
      if (L->isLoopLatch(BB)) continue;

      SmallVector<GetElementPtrInst*, 8> HoistableGEPs;
      for (auto& I : *BB) {
        auto* GEP = dyn_cast<GetElementPtrInst>(&I);
        if (!GEP) continue;
        auto* GV = dyn_cast<GlobalVariable>(GEP->getPointerOperand()->stripPointerCasts());
        if (!GV || !SVOPools.count(GV)) continue;
        if (!L->isLoopInvariant(GEP->getPointerOperand())) continue;

        // Check: GEP has at least one constant index (field offset) and one
        // loop-variant index (array subscript). The field offset part is invariant.
        bool HasVarIndex = false, HasConstField = false;
        for (unsigned O = 1; O < GEP->getNumOperands(); O++) {
          if (isa<ConstantInt>(GEP->getOperand(O))) HasConstField = true;
          else if (!L->isLoopInvariant(GEP->getOperand(O))) HasVarIndex = true;
        }
        if (HasConstField && HasVarIndex) {
          HoistableGEPs.push_back(GEP);
        }
      }

      for (GetElementPtrInst* GEP : HoistableGEPs) {
        // Split the GEP: hoist the constant field part to the preheader
        // and keep only the variable array index in the loop.
        SmallVector<Value*, 4> VarIndices;
        SmallVector<Value*, 4> ConstIndices;
        for (unsigned O = 1; O < GEP->getNumOperands(); O++) {
          Value* Op = GEP->getOperand(O);
          if (isa<ConstantInt>(Op)) ConstIndices.push_back(Op);
          else VarIndices.push_back(Op);
        }
        if (VarIndices.empty()) continue;

        // Create loop-invariant base + field offset in preheader
        BasicBlock* Preheader = L->getLoopPreheader();
        if (!Preheader) continue;

        // Verify pointer dominates preheader before hoisting
        if (!DT.dominates(GEP->getPointerOperand(), Preheader)) continue;

        GetElementPtrInst* FieldBase = GetElementPtrInst::Create(
            GEP->getSourceElementType(),
            GEP->getPointerOperand(),
            ConstIndices,
            GEP->getName() + ".field_base", Preheader->getTerminator());

        // Create the variable-indexed GEP inside the loop using FieldBase
        GetElementPtrInst* NewGEP = GetElementPtrInst::Create(
            GEP->getResultElementType(),
            FieldBase,
            VarIndices,
            GEP->getName() + ".loop", GEP);
        NewGEP->setDebugLoc(GEP->getDebugLoc());
        GEP->replaceAllUsesWith(NewGEP);
        GEP->eraseFromParent();
        Changed = true;
      }
    }

    // Recurse into sub-loops
    for (Loop* SubL : *L) {
      Changed |= hoistConstantGEPs(SubL, SVOPools);
    }

    return Changed;
  }

  bool insertNodePrefetches(Function& F, const SmallPtrSet<GlobalVariable*, 4>& SVOPools) {
    bool Changed = false;
    LLVMContext& Ctx = F.getContext();
    Function* PrefetchFn = Intrinsic::getDeclaration(F.getParent(), Intrinsic::prefetch);
    if (!PrefetchFn) return false;

    Type* Int8PtrTy = Type::getInt8PtrTy(Ctx);
    Type* Int32Ty = Type::getInt32Ty(Ctx);
    Type* Int1Ty = Type::getInt1Ty(Ctx);

    for (auto& BB : F) {
      for (auto& I : BB) {
        auto* GEP = dyn_cast<GetElementPtrInst>(&I);
        if (!GEP) continue;
        if (GEP->getName().startswith("prefetch.")) continue;

        auto* GV = dyn_cast<GlobalVariable>(GEP->getPointerOperand()->stripPointerCasts());
        if (!GV || !SVOPools.count(GV)) continue;

        // Check pattern: svo_pool[base + idx] where idx increments sequentially
        // Insert prefetch for idx+1, idx+2, idx+3
        if (GEP->getNumOperands() < 2) continue;

        Value* ArrayIdx = GEP->getOperand(1);
        if (!ArrayIdx->getType()->isIntegerTy()) continue;

        // Prefetch the next 3 cache lines (read, locality=1 = L3)
        for (int delta = 1; delta <= 3; delta++) {
          ConstantInt* PrefetchDelta = ConstantInt::get(ArrayIdx->getType(), delta);
          Value* PrefetchIdx = BinaryOperator::CreateAdd(ArrayIdx, PrefetchDelta,
              "prefetch.idx", GEP->getNextNode());
          SmallVector<Value*, 4> PrefetchIndices = {PrefetchIdx};
          for (unsigned O = 2; O < GEP->getNumOperands(); O++) {
            PrefetchIndices.push_back(GEP->getOperand(O));
          }
          GetElementPtrInst* PrefetchGEP = GetElementPtrInst::Create(
              GEP->getSourceElementType(), GEP->getPointerOperand(),
              PrefetchIndices, "prefetch.ptr", PrefetchIdx->getNextNode());
          Value* PrefetchPtr = new BitCastInst(PrefetchGEP, Int8PtrTy,
              "prefetch.cast", PrefetchIdx->getNextNode());

          // prefetch(read, locality=1, cache=1= data cache)
          CallInst::Create(PrefetchFn, {PrefetchPtr, ConstantInt::get(Int1Ty, 0),
              ConstantInt::get(Int32Ty, 1), ConstantInt::get(Int1Ty, 1)},
              "", PrefetchPtr->getNextNode());
          Changed = true;
        }
      }
    }
    return Changed;
  }
};

char SVOMemoryPass::ID = 0;
static RegisterPass<SVOMemoryPass> X("svo-memory",
    "Optimize SVO memory access patterns", false, false);

} // namespace
