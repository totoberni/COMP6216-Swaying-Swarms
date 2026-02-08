cmake_minimum_required(VERSION 3.20)
project(boid_swarm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    ${raygui_SOURCE_DIR}/src
)

target_link_libraries(boid_swarm PRIVATE
    flecs::flecs_static
    raylib
)

# --- Render demo executable ---
add_executable(render_demo
    src/render/render_demo.cpp
    src/render/renderer.cpp
)

target_include_directories(render_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${raygui_SOURCE_DIR}/src
    ${flecs_SOURCE_DIR}/include
)

target_link_libraries(render_demo PRIVATE
    raylib
    flecs::flecs_static
)

# --- Test executable ---
file(GLOB_RECURSE TEST_SOURCES
    tests/*.cpp
    tests/*.h
)

# Collect spatial module sources for testing
file(GLOB SPATIAL_SOURCES
    src/spatial/*.cpp
)

# Only build tests if there are test source files
if(TEST_SOURCES)
    add_executable(tests ${TEST_SOURCES} ${SPATIAL_SOURCES})

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
