# LLVM 17 Build with NVPTX Target
# Build command (from C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm-build-nvptx):

cd "C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm-build-nvptx"

# Configure with NVPTX target
"/c/Program Files/CMake/bin/cmake.exe" \
  "C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm-project-release-17.x/llvm" \
  -G "Visual Studio 18 2026" \
  -DCMAKE_INSTALL_PREFIX="C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm17-install" \
  -DLLVM_TARGETS_TO_BUILD="X86;NVPTX" \
  -DCMAKE_BUILD_TYPE=Release

# Build (takes 1-2 hours)
"/c/Program Files/CMake/bin/cmake.exe" --build . --config Release

# After build, run install
"/c/Program Files/CMake/bin/cmake.exe" --install . --config Release

# Verify NVPTX support
"C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm17-install/bin/llc.exe" --version | grep -i nvptx
