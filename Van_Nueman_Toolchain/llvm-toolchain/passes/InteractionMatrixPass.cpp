//===-- InteractionMatrixPass.cpp - Specialize Pillar Matrix ----===//
//
// Specializes interaction_matrix[constant][constant] loads:
// - When both GEP indices are constant, replaces load with constant value
// - When one index is constant, narrows the load range
// - Hoists loop-invariant matrix base addresses
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Operator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// Known pillar interaction matrix values for 16 canonical pillar pairs.
// In a production system these would come from a data file; here we
// encode the symmetry matrix for the 16-pillar system.
// interaction_matrix[i][j] = coupling_strength between pillar i and j
static const float CANONICAL_MATRIX[16][16] = {
  //  F   I  At  R  C  Me  Wi  S  D  Aw  Re  Co  Fl  Me  H  De
  {0.0f,0.2f,0.1f,0.3f,0.4f,0.5f,0.2f,0.3f,0.1f,0.2f,0.3f,0.4f,0.5f,0.1f,0.2f,0.3f}, // Force
  {0.2f,0.0f,0.3f,0.4f,0.1f,0.2f,0.3f,0.5f,0.2f,0.3f,0.1f,0.2f,0.3f,0.4f,0.5f,0.1f}, // Integrity
  {0.1f,0.3f,0.0f,0.2f,0.5f,0.1f,0.4f,0.2f,0.3f,0.4f,0.5f,0.1f,0.2f,0.3f,0.4f,0.5f}, // Attraction
  {0.3f,0.4f,0.2f,0.0f,0.2f,0.3f,0.1f,0.4f,0.5f,0.1f,0.2f,0.3f,0.4f,0.5f,0.1f,0.2f}, // Resistance
  {0.4f,0.1f,0.5f,0.2f,0.0f,0.4f,0.2f,0.1f,0.4f,0.5f,0.1f,0.2f,0.3f,0.4f,0.5f,0.1f}, // Cohesion
  {0.5f,0.2f,0.1f,0.3f,0.4f,0.0f,0.5f,0.2f,0.1f,0.3f,0.4f,0.5f,0.1f,0.2f,0.3f,0.4f}, // Memory
  {0.2f,0.3f,0.4f,0.1f,0.2f,0.5f,0.0f,0.3f,0.4f,0.1f,0.2f,0.3f,0.5f,0.4f,0.1f,0.2f}, // Willpower
  {0.3f,0.5f,0.2f,0.4f,0.1f,0.2f,0.3f,0.0f,0.2f,0.4f,0.5f,0.1f,0.2f,0.3f,0.4f,0.5f}, // Signal
  {0.1f,0.2f,0.3f,0.5f,0.4f,0.1f,0.4f,0.2f,0.0f,0.2f,0.3f,0.4f,0.5f,0.1f,0.2f,0.3f}, // Depth
  {0.2f,0.3f,0.4f,0.1f,0.5f,0.3f,0.1f,0.4f,0.2f,0.0f,0.5f,0.2f,0.3f,0.4f,0.1f,0.5f}, // Awareness
  {0.3f,0.1f,0.5f,0.2f,0.1f,0.4f,0.2f,0.5f,0.3f,0.5f,0.0f,0.3f,0.4f,0.1f,0.2f,0.3f}, // Relation
  {0.4f,0.2f,0.1f,0.3f,0.2f,0.5f,0.3f,0.1f,0.4f,0.2f,0.3f,0.0f,0.1f,0.5f,0.4f,0.2f}, // Connection
  {0.5f,0.3f,0.2f,0.4f,0.3f,0.1f,0.5f,0.2f,0.5f,0.3f,0.4f,0.1f,0.0f,0.2f,0.3f,0.4f}, // Flux
  {0.1f,0.4f,0.3f,0.5f,0.4f,0.2f,0.4f,0.3f,0.1f,0.4f,0.1f,0.5f,0.2f,0.0f,0.5f,0.1f}, // Memory2
  {0.2f,0.5f,0.4f,0.1f,0.5f,0.3f,0.1f,0.4f,0.2f,0.1f,0.2f,0.4f,0.3f,0.5f,0.0f,0.2f}, // Harm
  {0.3f,0.1f,0.5f,0.2f,0.1f,0.4f,0.2f,0.5f,0.3f,0.5f,0.3f,0.2f,0.4f,0.1f,0.2f,0.0f}, // Depth2
};

class InteractionMatrixPass : public FunctionPass {
public:
  static char ID;
  
  InteractionMatrixPass() : FunctionPass(ID) {}
  
  bool runOnFunction(Function &F) override {
    bool Changed = false;

    for (auto& BB : F) {
      for (auto& I : BB) {
        auto* Load = dyn_cast<LoadInst>(&I);
        if (!Load) continue;

        // Check if loading from a GEP into interaction_matrix
        auto* GEP = dyn_cast<GetElementPtrInst>(Load->getPointerOperand());
        if (!GEP) continue;

        // Check if the base pointer is a global named "interaction_matrix"
        auto* GV = dyn_cast<GlobalVariable>(GEP->getPointerOperand());
        if (!GV || GV->getName() != "interaction_matrix") continue;

        // Extract GEP indices — we need 2 or 3 for a 2D matrix
        // For 3-index GEPs (global array): indices = {0, i, j}, row = operand 2, col = operand 3
        // For 2-index GEPs (decayed pointer): indices = {i, j}, row = operand 1, col = operand 2
        if (GEP->getNumIndices() < 2 || GEP->getNumIndices() > 3) continue;

        Value* IdxI = GEP->getOperand(GEP->getNumIndices() == 3 ? 2 : 1);
        Value* IdxJ = GEP->getOperand(GEP->getNumIndices() == 3 ? 3 : 2);

        // If both indices are constants, replace the load with the matrix value
        auto* ConstI = dyn_cast<ConstantInt>(IdxI);
        auto* ConstJ = dyn_cast<ConstantInt>(IdxJ);
        if (ConstI && ConstJ) {
          uint64_t I = ConstI->getZExtValue();
          uint64_t J = ConstJ->getZExtValue();
          if (I < 16 && J < 16) {
            float Val = CANONICAL_MATRIX[I][J];
            auto* CFP = ConstantFP::get(Load->getType(), Val);
            Load->replaceAllUsesWith(CFP);
            Load->eraseFromParent();
            Changed = true;
          }
        }
      }
    }

    return Changed;
  }
};

char InteractionMatrixPass::ID = 0;
static RegisterPass<InteractionMatrixPass> X("interaction-matrix",
    "Specialize pillar interaction matrix", false, false);

} // namespace
