//===-- NVPTXExtensions.cpp - CUDA/NVPTX Extensions ------===//
//
// Van_Nueman-specific extensions for NVPTX (CUDA) backend.
// Maps pillar attributes to CUDA device functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicsNVPTX.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace vn {

// Add NVPTX-specific intrinsics for pillar compute
void addNVPTXExtensions(Module& M) {
  LLVMContext& C = M.getContext();

  // Declare __syncthreads() for barrier synchronization
  FunctionType* SyncTy = FunctionType::get(
      Type::getVoidTy(C), false);
  Function::Create(SyncTy,
                    Function::ExternalLinkage,
                    "__syncthreads", &M);

  // Declare __shared__ memory access intrinsics
  // For SVO node pool shared memory
  FunctionType* SharedTy = FunctionType::get(
      Type::getInt8PtrTy(C), {Type::getInt32Ty(C)}, false);
  Function::Create(SharedTy,
                    Function::ExternalLinkage,
                    "vulkan_svo_shared", &M);
}

// Convert pillar_vector attribute to NVPTX kernel
void convertToNVPTXKernel(Function& F) {
  if (!F.hasFnAttribute("pillar_vector")) {
    return;
  }

  // Add NVPTX kernel attribute
  F.addFnAttr("nvptx-kernel");

  // Mark as device function
  F.addFnAttr("device");

  errs() << "NVPTXExtensions: marked " << F.getName()
         << " as NVPTX kernel\n";
}

} // namespace vn
