cmake_minimum_required(VERSION 3.20)

# vcpkg toolchain - MUST be set before project()
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "Using VCPKG_ROOT: $ENV{VCPKG_ROOT}")
elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "Using vcpkg from C:/vcpkg")
elseif(EXISTS "C:/Microsoft Visual Studio/18/Professional/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/Microsoft Visual Studio/18/Professional/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "Using vcpkg from VS installation")
endif()

# Ensure PowerShell is findable for vcpkg's applocal post-build step.
# Find pwsh (PowerShell Core) or powershell (Windows PowerShell) and cache the path.
if(NOT DEFINED Z_VCPKG_POWERSHELL_PATH)
    find_program(Z_VCPKG_PWSH_PATH NAMES pwsh)
    if(Z_VCPKG_PWSH_PATH)
        set(Z_VCPKG_POWERSHELL_PATH "${Z_VCPKG_PWSH_PATH}" CACHE INTERNAL "Path to PowerShell implementation")
        message(STATUS "Using pwsh for vcpkg applocal: ${Z_VCPKG_POWERSHELL_PATH}")
    else()
        find_program(Z_VCPKG_POWERSHELL_PATH NAMES powershell PATHS "C:/Windows/System32/WindowsPowerShell/v1.0" NO_DEFAULT_PATH)
        if(NOT Z_VCPKG_POWERSHELL_PATH)
            find_program(Z_VCPKG_POWERSHELL_PATH NAMES powershell)
        endif()
        if(Z_VCPKG_POWERSHELL_PATH)
            set(Z_VCPKG_POWERSHELL_PATH "${Z_VCPKG_POWERSHELL_PATH}" CACHE INTERNAL "Path to PowerShell implementation")
            message(STATUS "Using powershell for vcpkg applocal: ${Z_VCPKG_POWERSHELL_PATH}")
        else()
            message(WARNING "vcpkg: PowerShell not found; applocal step will be skipped. "
                            "Set Z_VCPKG_POWERSHELL_PATH manually if needed.")
        endif()
    endif()
endif()

project(VanNuemanEngine CXX)

# Build type: 3=Release (default), 1=Debug
# Usage: cmake -DCMAKE_BUILD_TYPE=Release .. (or Debug)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Release=3, Debug=1)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE} (3=Release default, 1=Debug)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DNOMINMAX)

# Find packages
# Specify vcpkg cmake config path directly
if(EXISTS "C:/vcpkg/installed/x64-windows/share/glfw3")
    set(glfw3_DIR "C:/vcpkg/installed/x64-windows/share/glfw3")
    message(STATUS "Set glfw3_DIR manually")
endif()

if(EXISTS "C:/vcpkg/installed/x64-windows/share/imgui")
    set(imgui_DIR "C:/vcpkg/installed/x64-windows/share/imgui")
    message(STATUS "Set imgui_DIR manually")
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(CURL QUIET)
find_package(ZeroMQ CONFIG QUIET)
# vcpkg include path needed unconditionally (e.g. llm_worker.cpp uses zmq.h)
if(EXISTS "C:/vcpkg/installed/x64-windows/include")
    include_directories("C:/vcpkg/installed/x64-windows/include")
endif()

# Include directories
include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/renderer)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/api)
include_directories(${CMAKE_SOURCE_DIR}/physics)
include_directories(${CMAKE_SOURCE_DIR}/generated)

# Auto-generate PillarEnum.h, PillarConstraints.h, pillar_constants.py from pillars.yaml
# Generated files are committed; codegen runs automatically when pillars.yaml is newer.
set(PILLARS_YAML ${CMAKE_SOURCE_DIR}/pillars.yaml)
set(PILLAR_ENUM_H ${CMAKE_SOURCE_DIR}/generated/PillarEnum.h)
set(PILLAR_CONSTRAINTS_H ${CMAKE_SOURCE_DIR}/generated/PillarConstraints.h)
set(PILLAR_CONSTANTS_PY ${CMAKE_SOURCE_DIR}/generated/pillar_constants.py)
set(PILLAR_CODEGEN_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/generate_pillar_sources.py)

find_program(PILLAR_CODEGEN_PYTHON NAMES python3 python py)
if(PILLAR_CODEGEN_PYTHON)
    add_custom_command(
        OUTPUT ${PILLAR_ENUM_H} ${PILLAR_CONSTRAINTS_H} ${PILLAR_CONSTANTS_PY}
        DEPENDS ${PILLARS_YAML} ${PILLAR_CODEGEN_SCRIPT}
        COMMAND ${PILLAR_CODEGEN_PYTHON} ${PILLAR_CODEGEN_SCRIPT}
        COMMENT "Regenerating pillar sources from pillars.yaml..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_custom_target(pillar_codegen DEPENDS ${PILLAR_ENUM_H} ${PILLAR_CONSTRAINTS_H} ${PILLAR_CONSTANTS_PY})
    # Dependencies on van_nueman_game and VanNuemanTests added after their target definitions below
else()
    message(STATUS "Python not found; pillar codegen disabled (using pre-generated files)")
endif()
# whisper.cpp submodule (~650 files) — opt-in, default OFF
option(VN_USE_WHISPER "Enable whisper.cpp STT/TTS integration (submodule)" OFF)
if(VN_USE_WHISPER)
    add_subdirectory(whisper.cpp)
    include_directories(${CMAKE_SOURCE_DIR}/whisper.cpp/include)
    include_directories(${CMAKE_SOURCE_DIR}/whisper.cpp/ggml/include)
    message(STATUS "whisper.cpp enabled (VN_USE_WHISPER=ON)")
endif()
option(VN_USE_NATIVE_REASONING "Use native cognitive pipeline (Hopf-PID) instead of Ollama LLM worker" ON)
message(STATUS "Native reasoning: ${VN_USE_NATIVE_REASONING}")

include_directories(${CMAKE_SOURCE_DIR}/network)
include_directories(${CMAKE_SOURCE_DIR}/audio)
include_directories(${CMAKE_SOURCE_DIR}/database)
include_directories(${CMAKE_SOURCE_DIR}/neuroevolution)
include_directories(${CMAKE_SOURCE_DIR}/kernels)
include_directories(${CMAKE_SOURCE_DIR}/gui)
include_directories(${CMAKE_SOURCE_DIR}/gui/imgui)
include_directories(${CMAKE_SOURCE_DIR}/gui/backends)
include_directories(${CMAKE_SOURCE_DIR}/vn)
include_directories(${CMAKE_SOURCE_DIR}/game_play)
include_directories(${CMAKE_SOURCE_DIR}/audio)
include_directories(${CMAKE_SOURCE_DIR}/rendering)       # NEW
include_directories(${CMAKE_SOURCE_DIR}/neuro)             # NEW entity neural net
include_directories(${CMAKE_SOURCE_DIR}/biology)          # NEW
include_directories(${CMAKE_SOURCE_DIR}/scale)           # Phase 2
include_directories(${CMAKE_SOURCE_DIR}/voxel)           # Phase 4
include_directories(${CMAKE_SOURCE_DIR}/chemistry)      # Phase 8
include_directories(${CMAKE_SOURCE_DIR}/celestial)     # Phase 11
include_directories(${CMAKE_SOURCE_DIR}/consciousness) # Phase 13
include_directories(${CMAKE_SOURCE_DIR}/quantum)       # Phase 1: quantum integration
include_directories(${CMAKE_SOURCE_DIR}/fll/include)    # FLL Engine
include_directories(${CMAKE_SOURCE_DIR}/shared_memory/include)  # SHM IPC
include_directories(${CMAKE_SOURCE_DIR}/cognition)                # Phase 7: FUGURE
include_directories(${CMAKE_SOURCE_DIR}/vnes)                    # VNES Kelvin-cell ecosystem
#==================== SOURCE DEFINITIONS ====================

# Simulation library sources
set(SIMULATION_SOURCES
    simulation/tick_loop.cpp
)

# Scenarios
set(BOB_FEDERATED_SOURCES
    scenarios/bob_federated_scenario.cpp
)
add_executable(vn-bob-federated ${BOB_FEDERATED_SOURCES})
target_link_libraries(vn-bob-federated 
    vncore 
    vnphysics 
    vnnetwork 
    vnneuroevolution     vnapi 
    vnagents 
    vnscene 
    vnsimulation 
    vngameplay
    vnaudio
    vnrendering
    vnentityneuro
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
)

set(BOB_AGENT_SOURCES
    scenarios/bob_agent_scenario.cpp
)
add_executable(vn-bob-agent ${BOB_AGENT_SOURCES})
target_link_libraries(vn-bob-agent 
    vncore 
    vnphysics 
    vnnetwork 
    vnneuroevolution     vnapi 
    vnagents 
    vnscene 
    vnsimulation 
    vngameplay
    vnaudio
    vnrendering
    vnentityneuro
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
)

# WHT Inter-Agent Demo (separate executable)
set(WHT_DEMO_SOURCES
    scenarios/wht_inter_agent_demo.cpp
)
add_executable(vn-wht-demo ${WHT_DEMO_SOURCES})
target_link_libraries(vn-wht-demo 
    vncore 
    vnphysics 
    vnnetwork 
    vnneuroevolution     vnapi 
    vnagents 
    vnscene 
    vnsimulation 
    vngameplay
    vnaudio
    vnrendering
    vnentityneuro
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
)

# Voxel Evolution Demo (Phase 5: neuroevolution integration)
set(VOXEL_EVOLUTION_SOURCES
    scenarios/voxel_evolution_demo.cpp
)
add_executable(vn-voxel-evolution ${VOXEL_EVOLUTION_SOURCES})
target_link_libraries(vn-voxel-evolution
    vncore
    vnphysics
    vnnetwork
    vnneuroevolution
    vnapi
    vnagents
    vnscene
    vnsimulation
    vngameplay
    vnaudio
    vnrendering
    vnentityneuro
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
)

# Voxel Chemistry Demo (Phase 8: chemistry/molecule layer)
set(VOXEL_CHEMISTRY_SOURCES
    scenarios/voxel_chemistry_demo.cpp
)
add_executable(vn-chemistry-demo ${VOXEL_CHEMISTRY_SOURCES})
target_link_libraries(vn-chemistry-demo
    vncore
    vnphysics
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Voxel Environment Chemistry Demo (Phase 9: environmental chemistry)
set(VOXEL_ENVIRONMENT_SOURCES
    scenarios/voxel_environment_demo.cpp
)
add_executable(vn-environment-demo ${VOXEL_ENVIRONMENT_SOURCES})
target_link_libraries(vn-environment-demo
    vncore
    vnphysics
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Voxel Society & Technology Demo (Phase 10: society, tool use, material science)
set(VOXEL_SOCIETY_SOURCES
    scenarios/voxel_society_demo.cpp
)
add_executable(vn-society-demo ${VOXEL_SOCIETY_SOURCES})
target_link_libraries(vn-society-demo
    vncore
    vnphysics
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Voxel Celestial Demo (Phase 11: celestial formation, planetary differentiation, stellar fusion)
set(VOXEL_CELESTIAL_SOURCES
    scenarios/voxel_celestial_demo.cpp
)
add_executable(vn-celestial-demo ${VOXEL_CELESTIAL_SOURCES})
target_link_libraries(vn-celestial-demo
    vncore
    vnphysics
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Voxel Cosmological Demo (Phase 12: large-scale structure, expansion, WHT bridge)
set(VOXEL_COSMOLOGICAL_SOURCES
    scenarios/voxel_cosmological_demo.cpp
)
add_executable(vn-cosmological-demo ${VOXEL_COSMOLOGICAL_SOURCES})
target_link_libraries(vn-cosmological-demo
    vncore
    vnphysics
    vnaudio
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Voxel Consciousness Demo (Phase 13: dream engine, astral projection, metaphysical realms)
set(VOXEL_CONSCIOUSNESS_SOURCES
    scenarios/voxel_consciousness_demo.cpp
)
add_executable(vn-consciousness-demo ${VOXEL_CONSCIOUSNESS_SOURCES})
target_link_libraries(vn-consciousness-demo
    vncore
    vnphysics
    vnaudio
    vnneuroevolution
    vnapi
    vnbio
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
)

# Hopf-PID Interaction Engine Demo (Phase 2: Hopf-PID membrane projection)
set(HOPF_PID_DEMO_SOURCES
    scenarios/hopf_pid_demo.cpp
)
add_executable(vn-hopf-pid-demo ${HOPF_PID_DEMO_SOURCES})
target_link_libraries(vn-hopf-pid-demo
    vncore
    vnphysics
    vnnetwork
    vnneuroevolution
    vnapi
    vnagents
    vnscene
    vnsimulation
    vngameplay
    vnaudio
    vnrendering
    vnentityneuro
    vnbio
    Vulkan::Vulkan
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
    $<$<BOOL:${ZeroMQ_FOUND}>:libzmq>
)

# Scene library sources
set(SCENE_SOURCES
    scene/chunk.cpp
    scene/star_cluster.cpp
)

# Agents library sources (includes NEW agent_login.cpp)
set(AGENTS_SOURCES
    agents/cognition.cpp
    agents/perception.cpp
    agents/fractal_spawner.cpp
    agents/agent_login.cpp
    cognition/fugure/fugure.cpp
)


# Core library sources (optional PostgreSQL)
set(CORE_SOURCES
    scale/scale_router.cpp
)
if(PostgreSQL_FOUND)
    list(APPEND CORE_SOURCES
        database/persistence.cpp
        database/world_state.cpp
    )
endif()
if(NOT CORE_SOURCES)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp "")
    set(CORE_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp)
endif()

# Physics library sources (includes NEW energy_system.cpp)
set(PHYSICS_SOURCES
    physics/pillar_coupling.cpp
    physics/energy_system.cpp
    physics/deformation.cpp
)

# Network library sources (optional glfw3)
set(NETWORK_SOURCES
    network/feedback_loops.cpp
    network/federated.cpp
    network/causal_handshake.cpp
)
if(glfw3_FOUND)
    list(APPEND NETWORK_SOURCES
        network/server.cpp
        network/client.cpp
    )
endif()

# Neuroevolution library sources (original + voxel bridge + organism fitness)
set(NEUROEVOLUTION_SOURCES
    neuroevolution/population.cpp
    neuroevolution/fitness.cpp
    neuroevolution/operators.cpp
    neuroevolution/boundaries.cpp
    neuroevolution/voxel_neuro_bridge.cpp
    neuroevolution/organism_fitness.cpp
    neuroevolution/voxel_crispr_bridge.cpp
)
if(NOT NEUROEVOLUTION_SOURCES)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/empty_neuro.cpp "")
    set(NEUROEVOLUTION_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/empty_neuro.cpp)
endif()

# Entity Neural Network sources (NEW: GPU compute for entities)
set(ENTITY_NEURO_SOURCES
    neuro/entity_neural_net.cpp
)

# API sources
set(API_SOURCES
    api/pillars_api_simple.cpp
    api/skelly_api_simple.cpp
    api/llm_bridge.cpp
    api/rest_api.cpp
    api/websocket_api.cpp
)

# Main game executable sources (optional glfw3, Vulkan, etc.)
set(GAME_SOURCES
    main.cpp
    gui/hud.cpp
    gui/pillar_overlay.cpp
)
if(Vulkan_FOUND AND glfw3_FOUND)
    list(APPEND GAME_SOURCES renderer/vulkan_renderer.cpp)
endif()
if(glfw3_FOUND)
    list(APPEND GAME_SOURCES 
        gui/splash_screen.cpp
        gui/main_menu.cpp
        gui/settings.cpp
        gui/loading_screen.cpp
    )
endif()

# Audio library sources
set(AUDIO_SOURCES
    audio/audio_system.cpp
    audio/voice_system.cpp
    audio/wht.cpp
    audio/wht_comm.cpp
    audio/wht_packet.cpp
    audio/wht_scaled.cpp
)

# Rendering library sources (NEW: opacity renderer, creature renderer)
set(RENDERING_SOURCES
    rendering/opacity_renderer.cpp
    rendering/creature_renderer.cpp
    rendering/scale_organism_renderer.cpp
)

# Biology library sources (NEW: creature system, voxel-skelly bridge, voxel organism)
set(BIOLOGY_SOURCES
    biology/biological_system.cpp
    biology/creature_system.cpp
    biology/voxel_skelly_bridge.cpp
    biology/voxel_organism.cpp
)

# Simulation library sources (NEW: alife growth, attention loop, transform compute)
set(SIMULATION_SOURCES
    simulation/tick_loop.cpp
    simulation/alife_growth.cpp
    simulation/attention_loop.cpp
    simulation/transform_compute.cpp
    simulation/hopf_compute.cpp
    simulation/native_reasoning_worker.cpp
)
list(APPEND SIMULATION_SOURCES simulation/llm_worker.cpp)

# GUI library sources (NEW: collaboration window)
set(GUI_SOURCES
    gui/collaboration_window.cpp
)

#==================== LIBRARY CREATION (ONCE only!) ====================

# Create simulation library
add_library(vnsimulation STATIC ${SIMULATION_SOURCES})
target_link_libraries(vnsimulation PUBLIC vnfll)
if(VN_USE_NATIVE_REASONING)
    target_compile_definitions(vnsimulation PUBLIC VN_USE_NATIVE_REASONING=1)
endif()

# Create scene library
add_library(vnscene STATIC ${SCENE_SOURCES})

# Create agents library
add_library(vnagents STATIC ${AGENTS_SOURCES})

# Create core library
add_library(vncore STATIC ${CORE_SOURCES})

# Create physics library
add_library(vnphysics STATIC ${PHYSICS_SOURCES})

# Create network library
add_library(vnnetwork STATIC ${NETWORK_SOURCES})

# Create neuroevolution library (original)
add_library(vnneuroevolution STATIC ${NEUROEVOLUTION_SOURCES})

# Create entity neural network library (NEW)
add_library(vnentityneuro STATIC ${ENTITY_NEURO_SOURCES})

# Create API library
add_library(vnapi STATIC ${API_SOURCES})
if(CURL_FOUND)
    target_compile_definitions(vnapi PRIVATE HAS_CURL)
endif()

# Create audio library
add_library(vnaudio STATIC ${AUDIO_SOURCES})

# Create rendering library (NEW)
add_library(vnrendering STATIC ${RENDERING_SOURCES})

# Create biology library (NEW)
add_library(vnbio STATIC ${BIOLOGY_SOURCES})

# Create GUI library (NEW)
add_library(vngui STATIC ${GUI_SOURCES})

# Create VNES library (Kelvin-cell ecosystem)
set(VNES_SOURCES
    vnes/vnes_library.cpp
)
add_library(vnvnes STATIC ${VNES_SOURCES})

# Create gameplay library
set(GAMEPLAY_SOURCES
    game_play/crispr_vault.cpp
    game_play/simulation_chamber.cpp
    game_play/relay_station.cpp
    game_play/emergency_console.cpp
    game_play/inter_scale_feedback.cpp
)
add_library(vngameplay STATIC ${GAMEPLAY_SOURCES})

#==================== CUDA SUPPORT ====================
# Find CUDA toolkit (for includes, libraries, and nvcc path)
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    message(STATUS "CUDA found: ${CUDAToolkit_VERSION} at ${CUDAToolkit_BIN_DIR}")
    add_compile_definitions(VN_USE_CUDA)
    set(CUDA_NVCC_EXECUTABLE "${CUDAToolkit_BIN_DIR}/nvcc.exe" CACHE INTERNAL "nvcc compiler")
    include_directories(${CUDAToolkit_INCLUDE_DIRS})
else()
    message(STATUS "CUDA toolkit not found")
endif()

# CUDA-Q detection: user path → find_package → hardcoded fallback
set(CUDAQ_INSTALL_DIR "" CACHE PATH "Custom CUDA-Quantum install directory")

if(CUDAQ_INSTALL_DIR)
    # User specified a path — verify it
    if(EXISTS "${CUDAQ_INSTALL_DIR}/include/cudaq.h")
        set(VN_HAS_CUDA_QUANTUM TRUE CACHE INTERNAL "CUDA-Q found at user path")
        message(STATUS "CUDA-Q found at user path: ${CUDAQ_INSTALL_DIR}")
    else()
        message(WARNING "CUDA-Q not found at user path: ${CUDAQ_INSTALL_DIR}")
    endif()
else()
    find_package(cudaq QUIET)
    if(cudaq_FOUND)
        set(VN_HAS_CUDA_QUANTUM TRUE CACHE INTERNAL "CUDA-Q found via find_package(cudaq)")
        message(STATUS "CUDA-Q found via find_package(cudaq)")
    else()
        set(_CUDAQ_FALLBACK "C:/Projects/ugc-compiler/deps/cuda-quantum/install")
        if(EXISTS "${_CUDAQ_FALLBACK}/include/cudaq.h")
            set(CUDAQ_INSTALL_DIR "${_CUDAQ_FALLBACK}")
            set(VN_HAS_CUDA_QUANTUM TRUE CACHE INTERNAL "CUDA-Q found at fallback path")
            message(STATUS "CUDA-Q found at fallback path: ${CUDAQ_INSTALL_DIR}")
        endif()
    endif()
endif()

if(VN_HAS_CUDA_QUANTUM)
    if(CUDAQ_INSTALL_DIR)
        include_directories("${CUDAQ_INSTALL_DIR}/include")
        list(APPEND CUDAQ_LIBS
            "${CUDAQ_INSTALL_DIR}/lib/cudaq.lib"
            "${CUDAQ_INSTALL_DIR}/lib/cudaq-builder.lib"
            "${CUDAQ_INSTALL_DIR}/lib/cudaq-common.lib"
            "${CUDAQ_INSTALL_DIR}/lib/cudaq-chemistry.lib"
            "${CUDAQ_INSTALL_DIR}/lib/nvqir.lib"
        )
    else()
        list(APPEND CUDAQ_LIBS cudaq::cudaq cudaq::builder)
    endif()
    add_compile_definitions(VN_USE_CUDA_QUANTUM)
endif()

#==================== CUDA KERNEL COMPILATION ====================
# Custom compilation for .cu files using nvcc directly
# (MSBuild CUDA toolset not available in Build Tools environment)
if(CUDAToolkit_FOUND)
    # Compile a .cu file to a .obj with nvcc
    function(vn_compile_cu SOURCE OUTPUT)
        get_filename_component(CU_SRC_NAME "${SOURCE}" NAME_WE)
        set(CU_OBJ "${CMAKE_CURRENT_BINARY_DIR}/cuda_kernels/${CU_SRC_NAME}.obj")
        set(${OUTPUT} "${CU_OBJ}" PARENT_SCOPE)
        add_custom_command(
            OUTPUT "${CU_OBJ}"
            DEPENDS "${SOURCE}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/cuda_kernels"
            COMMAND "${CUDA_NVCC_EXECUTABLE}"
                -x cu

                -ccbin "${CMAKE_CXX_COMPILER}"
                -I"${CMAKE_SOURCE_DIR}/include"
                -I"${CMAKE_SOURCE_DIR}/generated"
                -I"${CMAKE_SOURCE_DIR}/kernels"
                -I"${CMAKE_SOURCE_DIR}/physics"
                -I"${CMAKE_SOURCE_DIR}/simulation"
                -I"${CMAKE_SOURCE_DIR}/biology"
                -I"${CMAKE_SOURCE_DIR}/api"
                -I"${CUDAToolkit_INCLUDE_DIRS}"
                -rdc=true
                -DNDEBUG -O3
                -Xcompiler "/MD"
                --generate-code arch=compute_86,code=sm_86
                -o "${CU_OBJ}" -c "${SOURCE}"
            COMMENT "Compiling CUDA kernel: ${SOURCE}"
        )
    endfunction()

    # Device link step: combines all relocatable device code into one object
    function(vn_device_link CUDA_OBJS OUTPUT)
        set(DLINK_OBJ "${CMAKE_CURRENT_BINARY_DIR}/cuda_kernels/vncuda_dlink.obj")
        add_custom_command(
            OUTPUT "${DLINK_OBJ}"
            DEPENDS ${CUDA_OBJS}
            COMMAND "${CUDA_NVCC_EXECUTABLE}"
                -dlink ${CUDA_OBJS}
                -o "${DLINK_OBJ}"
                -lcudart -lcudadevrt
                --generate-code arch=compute_86,code=sm_86
                -Xcompiler "/MD"
            COMMENT "Device-linking CUDA kernels: ${DLINK_OBJ}"
        )
        set(${OUTPUT} "${DLINK_OBJ}" PARENT_SCOPE)
    endfunction()

    # Engine CUDA kernel sources
    # EXCLUDED: shaders/*.cu (OpenCL)
    set(ENGINE_CUDA_KERNELS
        simulation/cellular_automata.cu
        fll/shaders/fll_resonance.cu
        kernels/wht_fp_kernel.cu
        kernels/voxel_svo.cu
        kernels/skelly_compute.cu
        kernels/pillars_compute.cu
        kernels/hopf_pid_kernel.cu
        kernels/render_image.cu
        physics/pillar_coupling.cu
        physics/deformation.cu
        physics/fractal_skelly.cu
        api/pillars_api.cu
        simulation/cuda_pillar_compute.cu
    )

    # Create object files for all engine CUDA kernels
    set(ENGINE_CUDA_OBJECTS "")
    foreach(CU_SRC ${ENGINE_CUDA_KERNELS})
        set(FULL_PATH "${CMAKE_SOURCE_DIR}/${CU_SRC}")
        if(EXISTS "${FULL_PATH}")
            vn_compile_cu("${FULL_PATH}" CU_OBJ_PATH)
            list(APPEND ENGINE_CUDA_OBJECTS "${CU_OBJ_PATH}")
        endif()
    endforeach()

    # Static library grouping all CUDA kernel objects
    add_library(vncuda STATIC ${ENGINE_CUDA_OBJECTS})
    target_link_libraries(vncuda PUBLIC CUDA::cudart)
    target_include_directories(vncuda PUBLIC ${CUDAToolkit_INCLUDE_DIRS})
    target_compile_definitions(vncuda PUBLIC VN_USE_CUDA)
    set_target_properties(vncuda PROPERTIES LINKER_LANGUAGE CXX)

    # Device link step: combine all relocatable device code into one object
    vn_device_link("${ENGINE_CUDA_OBJECTS}" VNCUDA_DLINK)
    set(VNCUDA_DLINK_OBJ "${VNCUDA_DLINK}" CACHE INTERNAL "CUDA device-linked object")
    add_custom_target(vncuda_dlink DEPENDS "${VNCUDA_DLINK_OBJ}")

    message(STATUS "CUDA kernels: ${ENGINE_CUDA_KERNELS}")
endif()

#==================== FLL ENGINE ====================
set(FLL_SOURCES
    fll/include/FractalNode.h
    fll/include/QuantumSeed.h
    fll/include/ArenaAllocator.h
    fll/include/SemanticCompiler.h
    fll/include/FLLQPU.h
    fll/include/FLLShaders.h
    fll/include/FLLRenderer.h
    fll/include/FLL.h
    fll/shaders/fll_qpu_kernels.cpp
)

add_library(vnfll STATIC ${FLL_SOURCES})
target_include_directories(vnfll PUBLIC ${CMAKE_SOURCE_DIR}/fll/include)
target_link_libraries(vnfll PUBLIC vncore)

# Link CUDA library to vnfll if available
if(TARGET vncuda)
    target_link_libraries(vnfll PUBLIC vncuda CUDA::cudart)
    target_compile_definitions(vnfll PUBLIC VN_USE_CUDA)
endif()

# Link CUDA-Q libraries to vnfll if available
if(CUDAQ_LIBS)
    target_link_libraries(vnfll PUBLIC ${CUDAQ_LIBS})
    target_compile_definitions(vnfll PUBLIC VN_USE_CUDA_QUANTUM)
endif()

# CUDA-Q kernel compilation (nvq++ for __qpu__ definitions)
if(VN_HAS_CUDA_QUANTUM)
    if(CUDAQ_INSTALL_DIR)
        set(_NVQPP_HINTS "${CUDAQ_INSTALL_DIR}/bin")
    else()
        set(_NVQPP_HINTS "")
    endif()
    find_program(NVQPP_EXECUTABLE NAMES nvq++ cudaq++
        HINTS ${_NVQPP_HINTS})
    if(NVQPP_EXECUTABLE)
        set(FLL_QPU_DEFS ${CMAKE_SOURCE_DIR}/fll/shaders/fll_qpu_kernel_defs.cpp)
        set(FLL_QPU_OBJ ${CMAKE_CURRENT_BINARY_DIR}/fll_qpu_kernel_defs.o)
        set(_FLL_QPU_FLAGS "-DVN_USE_CUDA_QUANTUM" "-I${CMAKE_SOURCE_DIR}/fll/include")
        if(CUDAQ_INSTALL_DIR)
            list(APPEND _FLL_QPU_FLAGS "-I${CUDAQ_INSTALL_DIR}/include")
        endif()
        add_custom_command(
            OUTPUT ${FLL_QPU_OBJ}
            DEPENDS ${FLL_QPU_DEFS}
            COMMAND bash "${NVQPP_EXECUTABLE}" --enable-mlir --target qpp-cpu
                    ${_FLL_QPU_FLAGS} -c ${FLL_QPU_DEFS}
            COMMENT "Compiling FLL CUDA-Q kernel definitions with nvq++"
        )
        target_sources(vnfll PRIVATE ${FLL_QPU_OBJ})
        message(STATUS "nvq++ found: ${NVQPP_EXECUTABLE}")
    endif()
endif()

#==================== MAIN EXECUTABLE ====================

# Create main executable
add_executable(van-nueman-game ${GAME_SOURCES})
if(TARGET pillar_codegen)
    add_dependencies(van-nueman-game pillar_codegen)
endif()

# Find PostgreSQL (optional)
find_package(PostgreSQL QUIET)

# Link libraries
target_link_libraries(van-nueman-game
    vncore
    vnphysics
    vnnetwork
    vnneuroevolution
    vnentityneuro
    vnapi
    vnagents
    vnscene
    vnsimulation
    vngameplay
    vnaudio
    vnrendering
    vnbio
    vngui
    vnfll
    vnvnes
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${PostgreSQL_FOUND}>:${PostgreSQL_LIBRARIES}>
    $<$<BOOL:${glfw3_FOUND}>:imgui::imgui>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
    $<$<BOOL:${ZeroMQ_FOUND}>:libzmq>
    ${CUDAQ_LIBS}
)
if(DEFINED VNCUDA_DLINK_OBJ)
    target_sources(van-nueman-game PRIVATE "${VNCUDA_DLINK_OBJ}")
    add_dependencies(van-nueman-game vncuda_dlink)
    # Also add dlink to all demo executables that link vnsimulation (→vnfll→vncuda)
    foreach(demo_target vn-bob-federated vn-bob-agent vn-wht-demo vn-voxel-evolution vn-hopf-pid-demo)
        if(TARGET ${demo_target})
            target_sources(${demo_target} PRIVATE "${VNCUDA_DLINK_OBJ}")
            add_dependencies(${demo_target} vncuda_dlink)
        endif()
    endforeach()
endif()

# Include PostgreSQL headers
if(PostgreSQL_FOUND)
    include_directories(${PostgreSQL_INCLUDE_DIRS})
    add_compile_definitions(HAVE_POSTGRESQL)
endif()

# Define HAVE_GLFW if found
if(glfw3_FOUND)
    add_compile_definitions(HAVE_GLFW)
endif()

# UGC SPIR-V pipeline for kernel compilation (replaces deprecated vncc)
set(UGC_EXECUTABLE "C:/Projects/ugc-compiler/build/Release/ugc.exe")
set(UGC_BIN_EXECUTABLE "C:/Projects/ugc-compiler/bin/ugc.exe")
if(EXISTS "${UGC_EXECUTABLE}")
    set(UGC_COMPILER "${UGC_EXECUTABLE}")
elseif(EXISTS "${UGC_BIN_EXECUTABLE}")
    set(UGC_COMPILER "${UGC_BIN_EXECUTABLE}")
else()
    set(UGC_COMPILER "")
endif()

# VCToolsInstallDir for UGC compiler (detect from environment or use fallback)
if(DEFINED ENV{VCToolsInstallDir})
    set(VCToolsInstallDir "$ENV{VCToolsInstallDir}")
else()
    set(VCToolsInstallDir "C:/Users/aobie/.vs/BuildTools/VC/Tools/MSVC/14.44.35207")
endif()

if(EXISTS "${UGC_COMPILER}")
    # Include paths for UGC kernel compilation:
    # 1) Toolchain vn/ headers (PillarTypes, ScaledInt, Fractal)
    # 2) CUDA-Q install headers for __qpu__ kernel support
    set(UGC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Van_Nueman_Toolchain/llvm-toolchain/include")
    set(UGC_CUDAQ_INCLUDE_DIR "${CUDAQ_INSTALL_DIR}/include")
    
    # Find all kernel source files (UGC compiles .cpp to SPIR-V)
    # Exclude render_vulkan.cpp (it's GLSL, not C++) and any non-vncc files
    file(GLOB KERNEL_SOURCES "${CMAKE_SOURCE_DIR}/kernels/*.cpp")
    list(FILTER KERNEL_SOURCES EXCLUDE REGEX "render_vulkan\\.cpp$")
    list(FILTER KERNEL_SOURCES EXCLUDE REGEX "voxel_svo\\.cpp$")
    
    # Collect SPV outputs for dependency tracking
    set(KERNEL_SPV_OUTPUTS "")
    
    # Make sure the kernels directory exists
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kernels/.dir"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/kernels"
    )
    
    # Compile each kernel .cpp to .spv using ugc-compiler
    foreach(KERNEL_SRC ${KERNEL_SOURCES})
        get_filename_component(KERNEL_NAME ${KERNEL_SRC} NAME_WE)
        set(KERNEL_SPV "${CMAKE_CURRENT_BINARY_DIR}/kernels/${KERNEL_NAME}.spv")
        list(APPEND KERNEL_SPV_OUTPUTS "${KERNEL_SPV}")
        
        # Include paths: toolchain vn/ headers (PillarEnumUGC.h copied by codegen)
        set(UGC_KERNEL_INCLUDE "${CMAKE_SOURCE_DIR}/Van_Nueman_Toolchain/llvm-toolchain/include")
        
        # Path to llvm-spirv (built from SPIRV-LLVM-Translator dependency)
        set(LLVM_SPIRV_EXECUTABLE "C:/Projects/ugc-compiler/deps/SPIRV-LLVM-Translator/build/tools/llvm-spirv/Release/llvm-spirv.exe")
        
        add_custom_command(
            OUTPUT ${KERNEL_SPV}
            COMMAND ${CMAKE_COMMAND} -E env
                "CUDAQ_INCLUDE_PATH=${UGC_KERNEL_INCLUDE}"
                "VCToolsInstallDir=${VCToolsInstallDir}"
                "UGC_LLVM_SPIRV_PATH=${LLVM_SPIRV_EXECUTABLE}"
                "${UGC_COMPILER}" "${KERNEL_SRC}" -o "${KERNEL_SPV}" --spv-version 1.6 -O3
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/scripts/validate_spirv.py"
                "${KERNEL_SPV}" --spv-version 1.6
            DEPENDS "${KERNEL_SRC}" "${UGC_COMPILER}" "${CMAKE_CURRENT_BINARY_DIR}/kernels/.dir"
            COMMENT "Compiling ${KERNEL_NAME} to SPIR-V using ugc-compiler"
        )
        
        # Copy generated SPIR-V to the game output directory
        add_custom_command(TARGET van-nueman-game POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -D SRC="${KERNEL_SPV}"
                -D DST="$<TARGET_FILE_DIR:van-nueman-game>/${KERNEL_NAME}.spv"
                -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        )
    endforeach()
    
    # Custom target that depends on all kernel SPV outputs
    add_custom_target(kernel_spv DEPENDS ${KERNEL_SPV_OUTPUTS})
    
    # Make game and test executables depend on kernel SPV compilation
    add_dependencies(van-nueman-game kernel_spv)
    if(TARGET VanNuemanTests)
        add_dependencies(VanNuemanTests kernel_spv)
    endif()
else()
    message(WARNING "ugc-compiler not found. Set UGC path or install from C:/Projects/ugc-compiler. Using pre-compiled SPIR-V fallback.")
endif()

# Copy SPIR-V shaders to build directory (existing pre-compiled shaders)
# When UGC compiler is present, kernel .spv files are produced by the UGC pipeline above
# and POST_BUILD copies them to the output dir. The pre-compiled copies below serve as
# fallback for GLSL shaders and for kernels when UGC isn't available.
if(Vulkan_FOUND)
    add_custom_command(TARGET van-nueman-game POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/render_vulkan.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/render_vulkan.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/fullscreen.vert.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/fullscreen.vert.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/display.frag.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/display.frag.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/transform_compute.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/transform_compute.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/fll/shaders/fll_glyph.comp.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/fll_glyph.comp.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/fll/shaders/fll_glyph.frag.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/fll_glyph.frag.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/render_skelly_sdf.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/render_skelly_sdf.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
        COMMAND ${CMAKE_COMMAND}
            -D SRC="${CMAKE_SOURCE_DIR}/kernels/hopf_pid_compute.spv"
            -D DST="$<TARGET_FILE_DIR:van-nueman-game>/hopf_pid_compute.spv"
            -P "${CMAKE_SOURCE_DIR}/scripts/copy_if_exists.cmake"
    )
endif()

# Optional: Single player
if(BUILD_SINGLE_PLAYER)
    add_executable(vn-single-player single_player/tutorial.cpp)
    target_link_libraries(vn-single-player vncore vnphysics)
endif()

# Optional: Server registry
if(BUILD_SERVER_REGISTRY)
    add_executable(vn-server-registry server_registry/approval_system.cpp)
    target_link_libraries(vn-server-registry vnnetwork vncore)
endif()

# Testing
enable_testing()
include(CTest)

# Test executable
add_executable(VanNuemanTests tests/test_main.cpp)
if(TARGET pillar_codegen)
    add_dependencies(VanNuemanTests pillar_codegen)
endif()
target_link_libraries(VanNuemanTests vncore vnphysics vnneuroevolution vnagents vnscene vnaudio vnapi vnsimulation vnfll $<$<BOOL:${CURL_FOUND}>:CURL::libcurl> ${CUDAQ_LIBS})
if(DEFINED VNCUDA_DLINK_OBJ)
    target_sources(VanNuemanTests PRIVATE "${VNCUDA_DLINK_OBJ}")
    add_dependencies(VanNuemanTests vncuda_dlink)
endif()
target_link_options(VanNuemanTests PRIVATE "LINKER:/STACK:2097152")

# Add test
add_test(NAME VanNuemanTests COMMAND VanNuemanTests)

# SPIR-V kernel compatibility static analysis (Python)
find_program(PYTHON_EXECUTABLE python)
if(PYTHON_EXECUTABLE)
    add_test(
        NAME KernelCompatCheck
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/check_kernel_compat.py"
    )
    set_tests_properties(KernelCompatCheck PROPERTIES
        FIXTURES_REQUIRED "kernel-compat"
    )
endif()

message(STATUS "Van Nueman Engine configuration:")
message(STATUS "  CXX Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Vulkan found: ${Vulkan_FOUND}")
message(STATUS "  GLFW found: ${glfw3_FOUND}")
message(STATUS "  Tests enabled: ${BUILD_TESTING}")
