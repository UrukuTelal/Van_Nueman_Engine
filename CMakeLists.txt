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

project(VanNuemanEngine CXX)

# Build type: 3=Release (default), 1=Debug
# Usage: cmake -DCMAKE_BUILD_TYPE=Release .. (or Debug)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Release=3, Debug=1)" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE} (3=Release default, 1=Debug)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Include directories
include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/renderer)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/api)
include_directories(${CMAKE_SOURCE_DIR}/physics)
include_directories(${CMAKE_SOURCE_DIR}/network)
include_directories(${CMAKE_SOURCE_DIR}/audio)
include_directories(${CMAKE_SOURCE_DIR}/whisper.cpp/include)
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
)

# Core library sources (optional PostgreSQL)
set(CORE_SOURCES)
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
)

# Network library sources (optional glfw3)
set(NETWORK_SOURCES
    network/feedback_loops.cpp
    network/federated.cpp
)
if(glfw3_FOUND)
    list(APPEND NETWORK_SOURCES
        network/server.cpp
        network/client.cpp
    )
endif()

# Neuroevolution library sources (original)
set(NEUROEVOLUTION_SOURCES
    neuroevolution/population.cpp
    neuroevolution/fitness.cpp
    neuroevolution/operators.cpp
    neuroevolution/boundaries.cpp
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
)

# Rendering library sources (NEW: opacity renderer, creature renderer)
set(RENDERING_SOURCES
    rendering/opacity_renderer.cpp
    rendering/creature_renderer.cpp
)

# Biology library sources (NEW: creature system)
set(BIOLOGY_SOURCES
    biology/biological_system.cpp
    biology/creature_system.cpp
)

# Simulation library sources (NEW: alife growth)
set(SIMULATION_SOURCES
    simulation/tick_loop.cpp
    simulation/alife_growth.cpp
)

# GUI library sources (NEW: collaboration window)
set(GUI_SOURCES
    gui/collaboration_window.cpp
)

#==================== LIBRARY CREATION (ONCE only!) ====================

# Create simulation library
add_library(vnsimulation STATIC ${SIMULATION_SOURCES})

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

# Create audio library
add_library(vnaudio STATIC ${AUDIO_SOURCES})

# Create rendering library (NEW)
add_library(vnrendering STATIC ${RENDERING_SOURCES})

# Create biology library (NEW)
add_library(vnbio STATIC ${BIOLOGY_SOURCES})

# Create GUI library (NEW)
add_library(vngui STATIC ${GUI_SOURCES})

# Create gameplay library
set(GAMEPLAY_SOURCES
    game_play/crispr_vault.cpp
    game_play/simulation_chamber.cpp
    game_play/relay_station.cpp
    game_play/emergency_console.cpp
    game_play/inter_scale_feedback.cpp
)
add_library(vngameplay STATIC ${GAMEPLAY_SOURCES})

#==================== MAIN EXECUTABLE ====================

# Create main executable
add_executable(van-nueman-game ${GAME_SOURCES})

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
    ${VULKAN_LIBRARIES}
    $<$<BOOL:${glfw3_FOUND}>:glfw>
    $<$<BOOL:${PostgreSQL_FOUND}>:${PostgreSQL_LIBRARIES}>
    $<$<BOOL:${glfw3_FOUND}>:imgui::imgui>
    $<$<BOOL:${CURL_FOUND}>:CURL::libcurl>
)

# Include PostgreSQL headers
if(PostgreSQL_FOUND)
    include_directories(${PostgreSQL_INCLUDE_DIRS})
    add_compile_definitions(HAVE_POSTGRESQL)
endif()

# Define HAVE_GLFW if found
if(glfw3_FOUND)
    add_compile_definitions(HAVE_GLFW)
endif()

# Custom LLVM -> SPIR-V pipeline for kernel compilation
set(VNCC_EXECUTABLE "${CMAKE_SOURCE_DIR}/Van_Nueman_Toolchain/llvm-toolchain/build/frontend/Release/vncc.exe")
if(EXISTS "${VNCC_EXECUTABLE}")
    # Find all kernel source files
    file(GLOB KERNEL_SOURCES "${CMAKE_SOURCE_DIR}/kernels/*.cpp")
    
    # Compile each kernel .cpp to .spv using vncc
    foreach(KERNEL_SRC ${KERNEL_SOURCES})
        get_filename_component(KERNEL_NAME ${KERNEL_SRC} NAME_WE)
        set(KERNEL_SPV "${CMAKE_CURRENT_BINARY_DIR}/kernels/${KERNEL_NAME}.spv")
        
        add_custom_command(
            OUTPUT ${KERNEL_SPV}
            COMMAND "${VNCC_EXECUTABLE}" -emit-spirv -I"${CMAKE_SOURCE_DIR}/vn" "${KERNEL_SRC}" -o "${KERNEL_SPV}"
            DEPENDS "${KERNEL_SRC}" "${VNCC_EXECUTABLE}"
            COMMENT "Compiling ${KERNEL_NAME}.cpp to SPIR-V using vncc"
        )
        
        # Make sure the kernels directory exists in build
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/kernels/.dir"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/kernels"
            DEPENDS ${KERNEL_SPV}
        )
        
        # Copy generated SPIR-V to build directory for the game executable
        add_custom_command(TARGET van-nueman-game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${KERNEL_SPV}"
                "$<TARGET_FILE_DIR:van-nueman-game>/kernels/${KERNEL_NAME}.spv"
        )
    endforeach()
else()
    message(WARNING "vncc.exe not found at ${VNCC_EXECUTABLE}. Skipping custom LLVM->SPIR-V pipeline.")
endif()

# Copy SPIR-V shaders to build directory (existing pre-compiled shaders)
if(Vulkan_FOUND)
    add_custom_command(TARGET van-nueman-game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/kernels/render_vulkan.spv"
            "$<TARGET_FILE_DIR:van-nueman-game>/render_vulkan.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/kernels/fullscreen.vert.spv"
            "$<TARGET_FILE_DIR:van-nueman-game>/fullscreen.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/kernels/display.frag.spv"
            "$<TARGET_FILE_DIR:van-nueman-game>/display.frag.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/neuro/entity_neural_net.spv"
            "$<TARGET_FILE_DIR:van-nueman-game>/entity_neural_net.spv"
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
target_link_libraries(VanNuemanTests vncore vnphysics vnneuroevolution vnagents vnscene)

# Add test
add_test(NAME VanNuemanTests COMMAND VanNuemanTests)

message(STATUS "Van Nueman Engine configuration:")
message(STATUS "  CXX Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Vulkan found: ${Vulkan_FOUND}")
message(STATUS "  GLFW found: ${glfw3_FOUND}")
message(STATUS "  Tests enabled: ${BUILD_TESTING}")
