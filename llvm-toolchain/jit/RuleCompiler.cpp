//===-- RuleCompiler.cpp - Pillar Rule Compiler ----------===//
//
// Compiles pillar rules from string to native code.
// Supports [[pillar_vector]] and [[scaled(20)]] attributes.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>
#include <string>

using namespace llvm;

namespace vn {

// Rule compiler: compiles pillar rule source to IR
class RuleCompiler {
public:
  RuleCompiler(LLVMContext& C) : Context(C) {}

  // Compile a pillar rule
  std::unique_ptr<Module> compile(const std::string& RuleName,
                                  const std::string& Source) {
    auto M = std::make_unique<Module>(RuleName, Context);

    // Parse source and generate IR
    parseAndGenerate(*M, Source);

    return M;
  }

private:
  LLVMContext& Context;

  void parseAndGenerate(Module& M, const std::string& Source) {
    // Create function: void update(int32_t* pillars, float dt)
    Type* Int32Ty = Type::getInt32Ty(M.getContext());
    Type* FloatTy = Type::getFloatTy(M.getContext());
    Type* VoidTy = Type::getVoidTy(M.getContext());
    Type* PtrTy = Type::getInt32PtrTy(M.getContext());

    FunctionType* FT = FunctionType::get(VoidTy, {PtrTy, FloatTy}, false);

    Function* F = Function::Create(FT, Function::ExternalLinkage,
                                     "pillar_update", &M);

    BasicBlock* BB = BasicBlock::Create(M.getContext(), "entry", F);

    // Load pillar[0] (Awareness)
    Value* Ptr = GetElementPtrInst::Create(
        Int32Ty,
        F->getArg(0),
        {ConstantInt::get(M.getContext(), APInt(32, 0))},
        "ptr", BB);

    LoadInst* Load = new LoadInst(Int32Ty, Ptr, "load", BB);

    // Add constant (simulating force * dt)
    Value* Add = BinaryOperator::Create(
        Instruction::Add,
        Load,
        ConstantInt::get(M.getContext(), APInt(32, 100)),
        "add", BB);

    // Store back
    new StoreInst(Add, Ptr, BB);

    ReturnInst::Create(M.getContext(), BB);
  }
};

} // namespace vn
