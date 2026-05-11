# Backend targets for Van_Nueman

# Backend targets for Van_Nueman
if(VN_BUILD_BACKEND)
  # SPIR-V Emitter with SPIRV-LLVM-Translator
  add_library(vn-spirv
    SPIRVEmitter.cpp
    NVPTXExtensions.cpp
    VnTargetMachine.cpp
  )

  if(NOT SPIRV_TRANSLATOR_DIR)
    set(SPIRV_TRANSLATOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../spirv-translator")
  endif()

  target_include_directories(vn-spirv
    PRIVATE
      ${LLVM_INCLUDE_DIRS}
      ${CLANG_INCLUDE_DIRS}
      ${CMAKE_CURRENT_SOURCE_DIR}/../include
      "${SPIRV_TRANSLATOR_DIR}/install/include"
  )

  target_link_libraries(vn-spirv
    PRIVATE
      LLVMCore
      LLVMIRWriter
      LLVMSupport
      "${SPIRV_TRANSLATOR_DIR}/install/lib/LLVMSPIRVLib.lib"
  )

  message(STATUS "Van_Nueman SPIR-V backend configured")
endif()
