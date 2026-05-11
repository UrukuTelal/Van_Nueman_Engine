# LLVM-Config.cmake - Additional LLVM configuration
# This provides helper functions for LLVM

# llvm_map_components_to_libnames - Map LLVM components to library names
macro(llvm_map_components_to_libnames out_var)
  foreach(component ${ARGN})
    if(component STREQUAL "all")
      set(${out_var} LLVM-C libclang)
    else()
      list(APPEND ${out_var} LLVM-C)
    endif()
  endforeach()
endmacro()

# llvm_process_sources - Process LLVM sources
macro(llvm_process_sources out_var)
  set(${out_var} ${ARGN})
endmacro()

message(STATUS "LLVM configured from: ${LLVM_BINARY_DIR}")
