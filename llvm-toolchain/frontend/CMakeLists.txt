# Frontend - Van Nueman C Compiler (vncc)

set(VNCC_SOURCES
  VnCompiler.cpp
  AttrPillar.cpp
  PillarSema.cpp
  FractalCodeGen.cpp
  ScaledInteger.cpp
)

# Build vncc as a Clang-based tool
add_executable(vncc ${VNCC_SOURCES})

# Link against LLVM and Clang libraries
target_link_libraries(vncc
  PRIVATE
    clangTooling
    clangFrontend
    clangDriver
    clangSerialization
    clangParse
    clangSema
    clangAST
    clangASTMatchers
    clangBasic
    clangLex
    LLVMCore
    LLVMSupport
)

# Install
install(TARGETS vncc DESTINATION bin)
