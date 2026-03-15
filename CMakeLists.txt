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

# --- Threading ---
find_package(Threads REQUIRED)

# --- OpenMP (optional — enables parallel loops in Phase 3+) ---
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
endif()

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
    Threads::Threads
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(boid_swarm PRIVATE OpenMP::OpenMP_CXX)
endif()

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
        Threads::Threads
        gtest_main
        gtest
    )
    if(OpenMP_CXX_FOUND)
        target_link_libraries(tests PRIVATE OpenMP::OpenMP_CXX)
    endif()

    enable_testing()
    add_test(NAME unit_tests COMMAND tests)
endif()

# --- CUDA GPU acceleration (optional) ---
option(USE_CUDA "Enable CUDA GPU acceleration" OFF)

if(USE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_ARCHITECTURES 86)  # RTX 3050 = SM_86

    file(GLOB_RECURSE CUDA_SOURCES src/gpu/*.cu)
    target_sources(boid_swarm PRIVATE ${CUDA_SOURCES})
    set_target_properties(boid_swarm PROPERTIES CUDA_ARCHITECTURES 86)
    target_compile_definitions(boid_swarm PRIVATE USE_CUDA)
    target_include_directories(boid_swarm PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

    # --- GPU test executable ---
    file(GLOB GPU_TEST_SOURCES tests/*.cu)
    if(GPU_TEST_SOURCES)
        include(GoogleTest)
        add_executable(gpu_tests ${GPU_TEST_SOURCES} ${CUDA_SOURCES} ${SPATIAL_SOURCES})
        target_include_directories(gpu_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
        )
        # spatial_grid.cpp uses raymath.h (inline Vector2 ops — no link needed)
        target_include_directories(gpu_tests SYSTEM PRIVATE
            ${raylib_SOURCE_DIR}/src
        )
        target_link_libraries(gpu_tests PRIVATE
            gtest_main
            gtest
        )
        set_target_properties(gpu_tests PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
        )
        gtest_discover_tests(gpu_tests)
    endif()
endif()
