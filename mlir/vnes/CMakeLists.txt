# VNES MLIR dialect pass — pluggable into mlir-opt
if(NOT MLIR_FOUND OR NOT VN_USE_MLIR)
    message(STATUS "Skipping vnes-mlir-dialect: MLIR not available")
    return()
endif()

# Source files
set(VNES_MLIR_SOURCES
    lib/VNESDialect.cpp
    lib/VNESFuseSolversPass.cpp
    lib/VNESLowerToLinalg.cpp
)

# Shared library plugin (loadable by mlir-opt --load-pass-plugin)
add_library(VNESMLIRDialect SHARED ${VNES_MLIR_SOURCES})

target_include_directories(VNESMLIRDialect
    BEFORE
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${MLIR_INCLUDE_DIRS}
)

target_link_libraries(VNESMLIRDialect
    PUBLIC
        MLIRIR
        MLIRSupport
        MLIRPass
        MLIRTransforms
        MLIRRewrite
        MLIRFuncDialect
        MLIRAsmParser
)

set_target_properties(VNESMLIRDialect
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mlir-plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mlir-plugins"
        PREFIX ""
)

if(WIN32)
    set_target_properties(VNESMLIRDialect PROPERTIES SUFFIX ".dll")
endif()

add_custom_target(vnes-mlir-dialect DEPENDS VNESMLIRDialect)

# Standalone test executable (uses MLIR directly, not plugin loading)
add_executable(VNESMLIRTest
    test/test_runner.cpp
    ${VNES_MLIR_SOURCES}
)

target_include_directories(VNESMLIRTest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${MLIR_INCLUDE_DIRS}
)

target_link_libraries(VNESMLIRTest
    PRIVATE
        MLIRIR
        MLIRSupport
        MLIRPass
        MLIRTransforms
        MLIRRewrite
        MLIRFuncDialect
        MLIRAsmParser
        MLIRArithDialect
        MLIRParser
)

# Custom mlir-opt replacement that statically links VNES dialect
# (avoids MSVC DLL TypeID boundary crash)
add_executable(vn-mlir-opt
    tools/vn_mlir_opt.cpp
    ${VNES_MLIR_SOURCES}
)

target_include_directories(vn-mlir-opt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${MLIR_INCLUDE_DIRS}
)

target_link_libraries(vn-mlir-opt
    PRIVATE
        MLIRIR
        MLIRSupport
        MLIRPass
        MLIRTransforms
        MLIRRewrite
        MLIRFuncDialect
        MLIRAsmParser
        MLIRArithDialect
        MLIRParser
        MLIRLinalgDialect
        MLIRTensorDialect
        MLIRLinalgTransforms
        MLIRMlirOptMain
)

# DIA SDK diaguids.lib hardcoded path (LLVMExports.cmake) fix for non-Enterprise VS
# The LLVM installed MLIR distribution embeds an absolute path to the DIA SDK at
# C:/Program Files/Microsoft Visual Studio/2022/Enterprise/DIA SDK/lib/amd64/diaguids.lib
# which doesn't exist when VS BuildTools is user-local. We override the link
# library path when found at the user-local BuildTools location.
if(WIN32 AND TARGET LLVMDebugInfoPDB)
    find_library(DIAGUIDS_LIB diaguids.lib PATHS
        "C:/Users/aobie/.vs/BuildTools/DIA SDK/lib/amd64"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/DIA SDK/lib/amd64"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/DIA SDK/lib/amd64"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/DIA SDK/lib/amd64"
    )
    if(DIAGUIDS_LIB)
        message(STATUS "Found diaguids.lib: ${DIAGUIDS_LIB}")
        get_target_property(LLVM_PDB_LIBS LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
        if(LLVM_PDB_LIBS)
            string(REPLACE
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/DIA SDK/lib/amd64/diaguids.lib"
                "${DIAGUIDS_LIB}"
                LLVM_PDB_LIBS_FIXED "${LLVM_PDB_LIBS}")
            set_target_properties(LLVMDebugInfoPDB PROPERTIES
                INTERFACE_LINK_LIBRARIES "${LLVM_PDB_LIBS_FIXED}")
        endif()
    else()
        message(WARNING "diaguids.lib not found — vn-mlir-opt may fail to link")
    endif()
endif()

add_test(NAME VNESMLIRTest COMMAND VNESMLIRTest)

# Test vn-mlir-opt on each test MLIR file
add_test(NAME vn-mlir-opt-test-dialect
    COMMAND vn-mlir-opt ${CMAKE_CURRENT_SOURCE_DIR}/test/test_vnes_dialect.mlir
            --verify-diagnostics --split-input-file)

add_test(NAME vn-mlir-opt-test-fusion
    COMMAND vn-mlir-opt
            "--pass-pipeline=builtin.module(func.func(vnes-fuse-solvers))"
            ${CMAKE_CURRENT_SOURCE_DIR}/test/test_darcy_flow.mlir
            --split-input-file)

add_test(NAME vn-mlir-opt-test-lowering
    COMMAND vn-mlir-opt
            "--pass-pipeline=builtin.module(func.func(vnes-lower-to-linalg))"
            ${CMAKE_CURRENT_SOURCE_DIR}/test/test_lower_to_linalg.mlir
            --split-input-file)
