# ── Fractal Logoramical Language (FLL) Engine ────────────────────────
# Target library for the FLL compiler and runtime environment.

add_library(fll STATIC
    include/FractalNode.h
    include/QuantumSeed.h
    include/ArenaAllocator.h
    include/SemanticCompiler.h
    include/FLLShaders.h
    include/FLLRenderer.h
)

target_include_directories(fll PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/fll>
)

target_link_libraries(fll PUBLIC
    vn_fp20        # fixed-point math
    vn_entity      # Entity UID system
)

# CUDA-Q optional
if(VN_USE_CUDA_QUANTUM)
    target_compile_definitions(fll PUBLIC VN_USE_CUDA_QUANTUM)
    target_include_directories(fll PUBLIC
        ${CUDA_QUANTUM_INCLUDE_DIRS}
    )
    target_link_libraries(fll PUBLIC
        ${CUDA_QUANTUM_LIBRARIES}
    )
endif()

# Vulkan optional (for rendering path)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_compile_definitions(fll PUBLIC VN_USE_VULKAN)
    target_include_directories(fll PUBLIC ${Vulkan_INCLUDE_DIRS})

    # GLSL shader compilation via glslc
    set(FLL_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
    set(FLL_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/fll_shaders)

    file(MAKE_DIRECTORY ${FLL_SPV_DIR})

    # Compute shader
    add_custom_command(
        OUTPUT ${FLL_SPV_DIR}/fll_glyph.comp.spv
        DEPENDS ${FLL_SHADER_DIR}/fll_glyph.comp
        COMMAND glslc -fshader-stage=compute
                ${FLL_SHADER_DIR}/fll_glyph.comp
                -o ${FLL_SPV_DIR}/fll_glyph.comp.spv
        COMMENT "Compiling FLL glyph compute shader"
    )

    # Fragment shader
    add_custom_command(
        OUTPUT ${FLL_SPV_DIR}/fll_glyph.frag.spv
        DEPENDS ${FLL_SHADER_DIR}/fll_glyph.frag
        COMMAND glslc -fshader-stage=fragment
                ${FLL_SHADER_DIR}/fll_glyph.frag
                -o ${FLL_SPV_DIR}/fll_glyph.frag.spv
        COMMENT "Compiling FLL glyph fragment shader"
    )

    add_custom_target(fll_shaders ALL
        DEPENDS
            ${FLL_SPV_DIR}/fll_glyph.comp.spv
            ${FLL_SPV_DIR}/fll_glyph.frag.spv
    )

    add_dependencies(fll fll_shaders)
endif()

# Install
install(TARGETS fll DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/fll)
