cmake_minimum_required(VERSION 3.20)
project(VanNuemanLLVMToolchain VERSION 1.0.0 LANGUAGES CXX)

# Find LLVM and Clang
find_package(LLVM 17 REQUIRED)
find_package(Clang 17 REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include LLVM directories
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(frontend)
add_subdirectory(passes)
add_subdirectory(backend)  # Phase 3
add_subdirectory(jit)      # Phase 4
add_subdirectory(test)

# Install targets
install(DIRECTORY include/vn DESTINATION include)
