//===-- VnCompiler.cpp - Van Nueman C Compiler Driver ------------------===//
//
// Main compiler driver that wraps Clang to support Van_Nueman-specific
// attributes: [[pillar_vector]], [[scaled(N)]], [[fractal]]
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <fstream>

int executeCommand(const std::vector<std::string>& cmd) {
  std::ostringstream oss;
  for (size_t i = 0; i < cmd.size(); i++) {
    oss << cmd[i];
    if (i < cmd.size() - 1) oss << " ";
  }
  return system(oss.str().c_str());
}

void printUsage(const char* progName) {
  std::cout << "Van Nueman C Compiler (vncc) - LLVM-based compiler\n";
  std::cout << "Usage: " << progName << " [options] <input files>\n";
  std::cout << "Options:\n";
  std::cout << "  -emit-llvm    Emit LLVM IR instead of native code\n";
  std::cout << "  -emit-ptx     Emit CUDA PTX assembly\n";
  std::cout << "  -emit-spirv   Emit SPIR-V bytecode\n";
  std::cout << "  -target <triple>  Set target triple\n";
  std::cout << "  -o <file>       Output file\n";
  std::cout << "  -O               Enable optimizations\n";
}

// Fix LLVM IR file triple for SPIR-V
bool fixLLForSpirV(const std::string& llFile) {
  std::ifstream in(llFile);
  if (!in) return false;
  
  std::string line;
  bool found = false;
  std::string content;
  
  while (std::getline(in, line)) {
    if (!found && line.find("target triple") != std::string::npos) {
      // Replace with SPIR-V triple
      size_t start = line.find("\"");
      size_t end = line.rfind("\"");
      if (start != std::string::npos && end != std::string::npos && start != end) {
        line.replace(start + 1, end - start - 1, "spir64-unknown-unknown");
        found = true;
      }
    }
    content += line + "\n";
  }
  in.close();
  
  if (found) {
    std::ofstream out(llFile);
    out << content;
    out.close();
    return true;
  }
  return false;
}

int main(int argc, const char** argv) {
  std::vector<std::string> args;
  std::string outputFile;
  bool emitLLVM = false;
  bool emitPTX = false;
  bool emitSPIRV = false;
  std::string targetTriple;
  std::vector<std::string> inputFiles;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-emit-llvm") == 0) {
      emitLLVM = true;
    } else if (strcmp(argv[i], "-emit-ptx") == 0) {
      emitPTX = true;
    } else if (strcmp(argv[i], "-emit-spirv") == 0) {
      emitSPIRV = true;
    } else if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
      targetTriple = argv[++i];
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      outputFile = argv[++i];
    } else if (strcmp(argv[i], "-O") == 0) {
      args.push_back("-O2");
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printUsage(argv[0]);
      return 0;
    } else {
      inputFiles.push_back(argv[i]);
    }
  }

  if (inputFiles.empty()) {
    std::cerr << "vncc: error: no input files\n";
    return 1;
  }

  const char* envClang = std::getenv("VN_CLANGPP");
  const char* envSpirv = std::getenv("VN_SPIRV_TOOL");
  const char* envDis   = std::getenv("VN_LLVM_DIS");
  const char* envAsm   = std::getenv("VN_LLVM_AS");

  std::string llvmBin = "C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm-project-release-17.x/build-vs18/Release/bin";
  std::string clangPP = envClang ? envClang : llvmBin + "/clang++.exe";
  std::string spirvTool = envSpirv ? envSpirv : "C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/spirv-translator/install/bin/llvm-spirv.exe";

  if (emitLLVM) {
    // LLVM IR output
    std::string llOut = outputFile.empty() ? "output.ll" : outputFile;
    std::vector<std::string> cmd;
    cmd.push_back(clangPP);
    cmd.push_back("-emit-llvm");
    cmd.push_back("-S");
    cmd.push_back("-c");
    cmd.push_back("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH");
    for (const auto& a : args) cmd.push_back(a);
    for (const auto& f : inputFiles) cmd.push_back(f);
    cmd.push_back("-o");
    cmd.push_back(llOut);
    
    std::cout << "vncc: Compiling to LLVM IR...\n";
    if (executeCommand(cmd) != 0) {
      std::cerr << "vncc: LLVM IR compilation failed\n";
      return 1;
    }
    std::cout << "vncc: LLVM IR output: " << llOut << "\n";
    return 0;
  }

  if (emitPTX) {
    // PTX output - use clang directly
    std::string ptxOut = outputFile.empty() ? "output.ptx" : outputFile;
    std::vector<std::string> cmd;
    cmd.push_back(clangPP);
    cmd.push_back("-target");
    cmd.push_back("nvptx64-nvidia-cuda");
    cmd.push_back("-S");
    cmd.push_back("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH");
    for (const auto& a : args) cmd.push_back(a);
    for (const auto& f : inputFiles) cmd.push_back(f);
    cmd.push_back("-o");
    cmd.push_back(ptxOut);
    
    std::cout << "vncc: Compiling to CUDA PTX...\n";
    if (executeCommand(cmd) != 0) {
      std::cerr << "vncc: PTX compilation failed\n";
      return 1;
    }
    std::cout << "vncc: PTX output: " << ptxOut << "\n";
    return 0;
  }

  if (emitSPIRV) {
    // SPIR-V: compile to LLVM IR, fix triple, then translate
    std::string llvmOut = outputFile.empty() ? "output.bc" : outputFile + ".bc";
    std::string llvmLL = outputFile.empty() ? "output.ll" : outputFile + ".ll";
    std::string spirvOut = outputFile.empty() ? "output.spv" : outputFile;
    
    // Step 1: Compile to LLVM IR bitcode
    std::vector<std::string> cmd;
    cmd.push_back(clangPP);
    cmd.push_back("-emit-llvm");
    cmd.push_back("-c");
    cmd.push_back("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH");
    for (const auto& a : args) cmd.push_back(a);
    for (const auto& f : inputFiles) cmd.push_back(f);
    cmd.push_back("-o");
    cmd.push_back(llvmOut);
    
    std::cout << "vncc: Compiling to LLVM IR for SPIR-V...\n";
    if (executeCommand(cmd) != 0) {
      std::cerr << "vncc: LLVM IR compilation failed\n";
      return 1;
    }
    
    // Step 2: Convert to text, fix triple, convert back to bitcode
    std::cout << "vncc: Fixing target triple for SPIR-V...\n";
    
    // Use llvm-dis to convert .bc to .ll
    std::vector<std::string> cmdDis;
    cmdDis.push_back(envDis ? envDis : llvmBin + "/llvm-dis.exe");
    cmdDis.push_back(llvmOut);
    cmdDis.push_back("-o");
    cmdDis.push_back(llvmLL);
    if (executeCommand(cmdDis) != 0) {
      std::cerr << "vncc: LLVM disassemble failed\n";
      return 1;
    }
    
    // Fix triple in .ll file
    if (!fixLLForSpirV(llvmLL)) {
      std::cerr << "vncc: Failed to fix LLVM IR for SPIR-V\n";
      return 1;
    }
    
    // Convert back to .bc
    std::vector<std::string> cmdAsm;
    cmdAsm.push_back(envAsm ? envAsm : llvmBin + "/llvm-as.exe");
    cmdAsm.push_back(llvmLL);
    cmdAsm.push_back("-o");
    cmdAsm.push_back(llvmOut);
    if (executeCommand(cmdAsm) != 0) {
      std::cerr << "vncc: LLVM assemble failed\n";
      return 1;
    }
    
    // Step 3: Translate to SPIR-V
    std::vector<std::string> cmd2;
    cmd2.push_back(spirvTool);
    cmd2.push_back(llvmOut);
    cmd2.push_back("-o");
    cmd2.push_back(spirvOut);
    
    std::cout << "vncc: Translating to SPIR-V...\n";
    if (executeCommand(cmd2) != 0) {
      std::cerr << "vncc: SPIR-V translation failed\n";
      return 1;
    }
    
    std::cout << "vncc: SPIR-V output: " << spirvOut << "\n";
    return 0;
  }

  // Default: native code compilation
  std::string outFile = outputFile.empty() ? "a.out" : outputFile;
  std::vector<std::string> cmd;
  cmd.push_back(clangPP);
  cmd.push_back("-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH");
  for (const auto& a : args) cmd.push_back(a);
  for (const auto& f : inputFiles) cmd.push_back(f);
  if (!outputFile.empty()) {
    cmd.push_back("-o");
    cmd.push_back(outFile);
  }
  
  std::cout << "vncc: Compiling to native code...\n";
  if (executeCommand(cmd) != 0) {
    std::cerr << "vncc: compilation failed\n";
    return 1;
  }
  std::cout << "vncc: output: " << outFile << "\n";
  return 0;
}
