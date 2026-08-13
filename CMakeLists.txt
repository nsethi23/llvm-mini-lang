cmake_minimum_required(VERSION 3.20)
project(mlang LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# LLVM
#
# Targets LLVM 18. See README.md for exact install steps on macOS/Linux.
# On macOS with Homebrew's keg-only llvm@18, point CMake at it explicitly:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix llvm@18)
# ---------------------------------------------------------------------------
find_package(LLVM 18.1 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(${LLVM_DIR}/AddLLVM.cmake)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

llvm_map_components_to_libnames(LLVM_LIBS core support)

enable_testing()

add_executable(mlang src/main.cpp)
target_include_directories(mlang PRIVATE include)
target_link_libraries(mlang PRIVATE ${LLVM_LIBS})

add_subdirectory(tests)
