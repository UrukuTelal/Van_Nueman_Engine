//===-- VnJIT.cpp - JIT Compilation Engine --------------===//
//
// Runtime JIT compilation for neuroevolution pillar rules.
// Uses LLVM OrcJIT to compile pillar rules at runtime.
//
//===----------------------------------------------------------------------===//

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include <memory>
#include <string>

using namespace llvm;
using namespace llvm::orc;

namespace vn {

// JIT compiler for Van_Nueman pillar rules
class VnJIT {
public:
  VnJIT() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetDisassembler();
    LLJIT = nullptr;
  }

  ~VnJIT() = default;

  // Initialize JIT (returns true on success)
  bool initialize() {
    if (auto JIT = LLJITBuilder().create()) {
      LLJIT = std::move(*JIT);
      return true;
    }
    return false;
  }

  // Compile pillar rule from source at runtime
  void* compileRule(const std::string& RuleName) {
    if (!LLJIT) return nullptr;

    auto Ctx = std::make_unique<LLVMContext>();
    auto M = std::make_unique<Module>(RuleName, *Ctx);
    createPillarUpdateFn(*M, RuleName);

    if (LLJIT->addIRModule(
            ThreadSafeModule(std::move(M), std::move(Ctx)))) {
      return nullptr;
    }

    if (auto Sym = LLJIT->lookup(RuleName)) {
      return reinterpret_cast<void*>(static_cast<uintptr_t>(Sym->getValue()));
    }
    return nullptr;
  }

private:
  std::unique_ptr<LLJIT> LLJIT;

  void createPillarUpdateFn(Module& M, const std::string& Name) {
    Type* Int32Ty = Type::getInt32Ty(M.getContext());
    Type* FloatTy = Type::getFloatTy(M.getContext());
    Type* VoidTy = Type::getVoidTy(M.getContext());
    Type* PtrTy = PointerType::getUnqual(M.getContext());

    FunctionType* FT = FunctionType::get(VoidTy, {PtrTy, FloatTy}, false);
    Function* F = Function::Create(FT, Function::ExternalLinkage, Name, &M);
    BasicBlock* BB = BasicBlock::Create(M.getContext(), "entry", F);
    ReturnInst::Create(M.getContext(), BB);
  }
};

} // namespace vn
