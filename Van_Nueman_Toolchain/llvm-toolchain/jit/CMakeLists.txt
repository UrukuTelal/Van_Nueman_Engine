# JIT engine for Van_Nueman

# JIT support
add_library(vn-jit
  VnJIT.cpp
  RuleCompiler.cpp
  NeuroevolutionJIT.cpp
)

target_include_directories(vn-jit
  PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(vn-jit
  PRIVATE
    LLVMOrcJIT
    LLVMExecutionEngine
    LLVMCore
    LLVMSupport
    LLVMMIRReader
)

message(STATUS "Van_Nueman JIT engine configured")
