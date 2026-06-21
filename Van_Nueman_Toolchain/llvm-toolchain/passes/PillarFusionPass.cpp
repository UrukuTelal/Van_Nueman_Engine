//===-- PillarFusionPass.cpp - Pillar Interaction Fusion ----------===//
//
// Fuses pillar interaction loops, eliminates redundant matrix lookups.
// Optimizes the 14x14 pillar interaction matrix accesses.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include <map>
#include <set>

using namespace llvm;

namespace {

// Pillar interaction matrix indices
// This pass fuses loops that access the interaction matrix
// to eliminate redundant lookups
class PillarFusionPass : public FunctionPass {
public:
  static char ID;
  PillarFusionPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    bool Changed = false;
    
    // Look for interaction_matrix accesses
    // Pattern: interaction_matrix[i][j] used multiple times
    std::map<std::pair<Value*, Value*>, Value*> MatrixLoads;
    
    for (BasicBlock &BB : F) {
      for (auto It = BB.begin(); It != BB.end(); ) {
        Instruction &I = *It;
        ++It;  // advance before potential erase
        
        // Check for load from interaction_matrix
        LoadInst *LI = dyn_cast<LoadInst>(&I);
        if (!LI) continue;
        
        // Check if this is an interaction matrix access
        if (isInteractionMatrixAccess(LI->getPointerOperand())) {
          // Get the indices
          std::pair<Value*, Value*> Indices = getMatrixIndices(LI->getPointerOperand());
          if (Indices.first && Indices.second) {
            auto Key = Indices;
            if (MatrixLoads.count(Key)) {
              // Found duplicate - replace this load with previous
              LI->replaceAllUsesWith(MatrixLoads[Key]);
              LI->eraseFromParent();
              Changed = true;
            } else {
              MatrixLoads[Key] = &I;
            }
          }
        }
      }
    }
    
    if (Changed) {
      errs() << "PillarFusionPass: Fused interaction matrix loads in "
             << F.getName() << "\n";
    }
    
    return Changed;
  }

private:
  bool isInteractionMatrixAccess(Value *Ptr) {
    // Check if Ptr is accessing interaction_matrix
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
      Value *Base = GEP->getPointerOperand();
      // Check if Base is interaction_matrix
      if (LoadInst *BaseLoad = dyn_cast<LoadInst>(Base)) {
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(BaseLoad->getPointerOperand())) {
          if (GV->getName().contains("interaction_matrix")) {
            return true;
          }
        }
      }
    }
    return false;
  }

  std::pair<Value*, Value*> getMatrixIndices(Value *Ptr) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
      if (GEP->getNumIndices() >= 2) {
        Value *Idx1 = GEP->getOperand(1);
        Value *Idx2 = GEP->getOperand(2);
        return {Idx1, Idx2};
      }
    }
    return {nullptr, nullptr};
  }
};

char PillarFusionPass::ID = 0;
static RegisterPass<PillarFusionPass> X("pillar-fusion",
    "Fuse pillar interaction matrix loads", false, false);

} // namespace
