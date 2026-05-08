//===-- SPIRVEmitter.cpp - SPIR-V Emitter via Translator -===//
//
// Emits SPIR-V binary using Khronos SPIRV-LLVM-Translator.
// Converts LLVM IR to SPIR-V for Vulkan compute shaders.
//
//===----------------------------------------------------------------------===//

#include "LLVMSPIRVLib/LLVMSPIRVLib.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include <string>
#include <sstream>

using namespace llvm;

namespace vn {

// Convert LLVM module to SPIR-V binary
bool emitSPIRV(Module* M, const std::string& OutputFile) {
  if (!M) return false;
  
  // Use SPIRV-LLVM-Translator to convert LLVM IR → SPIR-V
  std::string ErrMsg;
  std::ostringstream OS;
  
  // Translate LLVM IR to SPIR-V
  if (!writeSpirv(M, OS, ErrMsg)) {
    errs() << "SPIR-V translation failed: " << ErrMsg << "\n";
    return false;
  }
  
  // Write SPIR-V binary to file
  int FD;
  if (std::error_code EC = sys::fs::openFileForWrite(OutputFile, FD)) {
    errs() << "Failed to open output file: " << EC.message() << "\n";
    return false;
  }
  
  std::string Content = OS.str();
  raw_fd_ostream Out(FD, true);
  Out << Content;
  
  errs() << "SPIR-V emitted: " << OutputFile << " (" 
         << Content.size() << " bytes)\n";
  return true;
}

} // namespace vn
