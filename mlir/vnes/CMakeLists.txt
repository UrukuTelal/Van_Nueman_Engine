# VNES MLIR dialect pass — pluggable into mlir-opt
if(NOT MLIR_FOUND OR NOT VN_USE_MLIR)
    message(STATUS "Skipping vnes-mlir-dialect: MLIR not available")
    return()
endif()

# Source files
set(VNES_MLIR_SOURCES
    lib/VNESDialect.cpp
    lib/VNESFuseSolversPass.cpp
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

add_test(NAME VNESMLIRTest COMMAND VNESMLIRTest)
