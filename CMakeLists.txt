# 3.20+ required for C++23
cmake_minimum_required(VERSION 3.20)

project(BricktertainmentSystem
    VERSION 0.1.0
    DESCRIPTION "The Bricktertainment System - an NES emulator"
    LANGUAGES CXX
)

# Use C++23 and require it (no silent fallback to an older standard)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so editors/language servers resolve include paths
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a debug build if the user didn't pick one
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()


# Tests
enable_testing()

add_executable(test_bus tests/test_bus.cpp src/Bus.cpp src/Cartridge.cpp)
target_include_directories(test_bus PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_bus COMMAND test_bus)

add_executable(test_cpu tests/test_cpu.cpp src/CPU.cpp src/Bus.cpp src/Cartridge.cpp)
target_include_directories(test_cpu PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_cpu COMMAND test_cpu)

add_executable(test_cartridge tests/test_cartridge.cpp src/Cartridge.cpp)
target_include_directories(test_cartridge PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_cartridge COMMAND test_cartridge)

add_executable(test_ppu_bus tests/test_ppu_bus.cpp src/PPUBus.cpp src/Cartridge.cpp)
target_include_directories(test_ppu_bus PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_ppu_bus COMMAND test_ppu_bus)
