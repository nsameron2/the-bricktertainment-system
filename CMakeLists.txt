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

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()


# Tests
enable_testing()

add_executable(test_apu tests/test_apu.cpp src/APU.cpp src/CPUBus.cpp src/Controller.cpp src/PPU.cpp src/PPUBus.cpp src/Cartridge.cpp)
target_include_directories(test_apu PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(test_apu PRIVATE SDL3::SDL3)

add_test(NAME test_apu COMMAND test_apu)

add_executable(test_cpu_bus tests/test_cpu_bus.cpp src/CPUBus.cpp src/Controller.cpp src/PPU.cpp src/PPUBus.cpp src/Cartridge.cpp src/APU.cpp)
target_include_directories(test_cpu_bus PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(test_cpu_bus PRIVATE SDL3::SDL3)

add_test(NAME test_cpu_bus COMMAND test_cpu_bus)

add_executable(test_cpu tests/test_cpu.cpp src/CPU.cpp src/CPUBus.cpp src/Controller.cpp src/PPU.cpp src/PPUBus.cpp src/Cartridge.cpp src/APU.cpp)
target_include_directories(test_cpu PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(test_cpu PRIVATE SDL3::SDL3)

add_test(NAME test_cpu COMMAND test_cpu)

add_executable(test_cartridge tests/test_cartridge.cpp src/Cartridge.cpp)
target_include_directories(test_cartridge PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_cartridge COMMAND test_cartridge)

add_executable(test_ppu_bus tests/test_ppu_bus.cpp src/PPUBus.cpp src/Cartridge.cpp)
target_include_directories(test_ppu_bus PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_ppu_bus COMMAND test_ppu_bus)

add_executable(test_ppu tests/test_ppu.cpp src/PPU.cpp src/PPUBus.cpp src/Cartridge.cpp)
target_include_directories(test_ppu PRIVATE "${CMAKE_SOURCE_DIR}/include")

add_test(NAME test_ppu COMMAND test_ppu)
