# LLVM Passes for Van_Nueman

# Pillar optimization passes
add_library(vn-passes
  PillarFusionPass.cpp
  ScaledIntSimplifyPass.cpp
  InteractionMatrixPass.cpp
  LogDecayOptPass.cpp
  SVOMemoryPass.cpp
  FractalInlinePass.cpp
)

target_include_directories(vn-passes
  PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(vn-passes
  PRIVATE
    LLVMCore
    LLVMIRReader
    LLVMIRWriter
    LLVMSupport
    LLVMAnalysis
    LLVMScalarOpts
    LLVMLoopInfo
)

message(STATUS "Van_Nueman LLVM passes configured")
