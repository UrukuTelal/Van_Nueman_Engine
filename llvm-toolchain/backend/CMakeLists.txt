# Backend targets for Van_Nueman

# Backend targets for Van_Nueman
if(VN_BUILD_BACKEND)
  # SPIR-V Emitter with SPIRV-LLVM-Translator
  add_library(vn-spirv
    SPIRVEmitter.cpp
    NVPTXExtensions.cpp
    VnTargetMachine.cpp
  )

  target_include_directories(vn-spirv
    PRIVATE
      ${LLVM_INCLUDE_DIRS}
      ${CLANG_INCLUDE_DIRS}
      ${CMAKE_CURRENT_SOURCE_DIR}/../include
      "C:/Projects/Van_Nueman/spirv-translator/install/include"
  )

  target_link_libraries(vn-spirv
    PRIVATE
      LLVMCore
      LLVMIRWriter
      LLVMSupport
      "C:/Projects/Van_Nueman/spirv-translator/install/lib/LLVMSPIRVLib.lib"
  )

  message(STATUS "Van_Nueman SPIR-V backend configured")
endif()
