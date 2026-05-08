# LLVMConfig.cmake - Generated for LLVM 22.1.4
# This file allows CMake's find_package(LLVM) to work

set(LLVM_PACKAGE_VERSION "22.1.4")
set(LLVM_VERSION_MAJOR 22)
set(LLVM_VERSION_MINOR 1)
set(LLVM_VERSION_PATCH 4)
set(PACKAGE_VERSION "22.1.4")

set(LLVM_INSTALL_PREFIX "C:/Program Files/LLVM")
set(LLVM_BINARY_DIR "${LLVM_INSTALL_PREFIX}")
set(LLVM_LIBRARY_DIR "${LLVM_INSTALL_PREFIX}/lib")
set(LLVM_INCLUDE_DIR "${LLVM_INSTALL_PREFIX}/include")
set(LLVM_TOOLS_BINARY_DIR "${LLVM_INSTALL_PREFIX}/bin")

# Library directory
set(LLVM_LIBRARY_DIRS "${LLVM_LIBRARY_DIR}")
set(LLVM_INCLUDE_DIRS "${LLVM_INCLUDE_DIR}")

# Set available libraries
set(LLVM_AVAILABLE_LIBS
  libclang
  LLVM-C
  LTO
  Remarks
)

# Create imported targets for key libraries
if(NOT TARGET LLVM)
  add_library(LLVM SHARED IMPORTED)
  set_target_properties(LLVM PROPERTIES
    IMPORTED_LOCATION "${LLVM_BINARY_DIR}/bin/libLLVM.dll"
    IMPORTED_IMPLIB "${LLVM_LIBRARY_DIR}/LLVM-C.lib"
  )
endif()

if(NOT TARGET clang)
  add_library(clang SHARED IMPORTED)
  set_target_properties(clang PROPERTIES
    IMPORTED_LOCATION "${LLVM_BINARY_DIR}/bin/libclang.dll"
    IMPORTED_IMPLIB "${LLVM_LIBRARY_DIR}/libclang.lib"
  )
endif()

# LLVM defines
set(LLVM_DEFINITIONS "")
set(LLVM_ENABLE_RTTI OFF)
set(LLVM_ENABLE_EH OFF)
set(LLVM_TARGETS_TO_BUILD "X86")

# Find components (simplified)
macro(llvm_map_components_to_libnames out_var)
  set(${out_var} LLVM-C libclang)
endmacro()

# Include additional config if exists
include("${LLVM_BINARY_DIR}/lib/cmake/llvm/LLVM-Config.cmake" OPTIONAL)
