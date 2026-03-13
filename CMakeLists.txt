cmake_minimum_required(VERSION 3.20)
project(boid_swarm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build for optimization (-O2/-O3)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- CPM.cmake ---
# Use a short cache path to avoid Windows MAX_PATH issues with long project paths
if(WIN32 AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
    set(ENV{CPM_SOURCE_CACHE} "C:/.cpm")
endif()

include(cmake/CPM.cmake)

# --- Dependencies ---
CPMAddPackage(
    NAME flecs
    GITHUB_REPOSITORY SanderMertens/flecs
    GIT_TAG v4.1.4
)

CPMAddPackage(
    NAME raylib
    GITHUB_REPOSITORY raysan5/raylib
    GIT_TAG 5.5
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
)

CPMAddPackage(
    NAME raygui
    GITHUB_REPOSITORY raysan5/raygui
    GIT_TAG 4.0
)

CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    OPTIONS
        "INSTALL_GTEST OFF"
        "BUILD_GMOCK OFF"
)

# --- Collect sources ---
file(GLOB_RECURSE MAIN_SOURCES
    src/*.cpp
    src/*.h
)

# Exclude render_demo.cpp from main sources
list(FILTER MAIN_SOURCES EXCLUDE REGEX ".*render_demo\\.cpp$")

# --- Main executable ---
add_executable(boid_swarm ${MAIN_SOURCES})

target_include_directories(boid_swarm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(boid_swarm SYSTEM PRIVATE
    ${raygui_SOURCE_DIR}/src
)

target_link_libraries(boid_swarm PRIVATE
    flecs::flecs_static
    raylib
)

# Optimization flags (compatible with GCC, Clang, MSVC)
if(MSVC)
    target_compile_options(boid_swarm PRIVATE $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(boid_swarm PRIVATE $<$<CONFIG:Release>:-O2>)
endif()

# --- Test executable ---
file(GLOB_RECURSE TEST_SOURCES
    tests/*.cpp
    tests/*.h
)

# Collect spatial module sources for testing
file(GLOB SPATIAL_SOURCES
    src/spatial/*.cpp
)

# Collect sim module sources for testing (needed by test_config_loader)
file(GLOB SIM_SOURCES
    src/sim/*.cpp
)

# Collect ECS module sources for testing (needed by test_cure)
file(GLOB ECS_SOURCES
    src/ecs/*.cpp
)

# Only build tests if there are test source files
if(TEST_SOURCES)
    add_executable(tests ${TEST_SOURCES} ${SPATIAL_SOURCES} ${SIM_SOURCES} ${ECS_SOURCES})

    target_include_directories(tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(tests PRIVATE
        flecs::flecs_static
        raylib
        gtest_main
        gtest
    )

    enable_testing()
    add_test(NAME unit_tests COMMAND tests)
endif()
