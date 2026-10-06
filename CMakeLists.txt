cmake_minimum_required(VERSION 3.20)

project(
    Outland
    VERSION 0.2.0
    DESCRIPTION "Offline 3D open-world combat sandbox"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    OUTLAND_BUILD_TESTS
    "Build OUTLAND automated tests"
    ON
)

find_package(
    PkgConfig
    REQUIRED
)

pkg_check_modules(
    RAYLIB
    REQUIRED
    raylib
)

# ============================================================
# CORE
# ============================================================

add_library(
    outland_core
    STATIC

    src/engine/core/Game.cpp
    src/engine/core/GameTime.cpp

    src/game/player/Player.cpp
    src/game/combat/Health.cpp

    src/world/World.cpp
)

target_include_directories(
    outland_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(
    outland_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# ============================================================
# RENDERER
# ============================================================

add_library(
    outland_render
    STATIC

    src/engine/render/Renderer.cpp
    src/world/VerdaRegion.cpp
    src/dev/DevLab.cpp
    src/game/HomeScreen.cpp
    src/game/GameMode.cpp
    src/world/foliage/FoliageSystem.cpp
    src/player/VerdanCharacter.cpp
    src/world/physics/WorldCollision.cpp
    src/world/assets/VerdanArchitecture.cpp
    src/world/assets/VerdanProps.cpp
    src/world/terrain/TerrainHeight.cpp
    src/world/terrain/TerrainChunk.cpp
    src/world/terrain/TerrainWorld.cpp

    src/input/InputSystem.cpp
    src/input/TouchHUD.cpp
)

target_include_directories(
    outland_render
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include

    PRIVATE
        ${RAYLIB_INCLUDE_DIRS}
)

target_link_directories(
    outland_render
    PRIVATE
        ${RAYLIB_LIBRARY_DIRS}
)

target_link_libraries(
    outland_render
    PUBLIC
        outland_core

    PRIVATE
        ${RAYLIB_LIBRARIES}
)

target_compile_options(
    outland_render
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        ${RAYLIB_CFLAGS_OTHER}
)

# ============================================================
# EXECUTABLE
# ============================================================

add_executable(
    outland
    src/main.cpp
)

target_link_libraries(
    outland
    PRIVATE
        outland_core
        outland_render
)

# ============================================================
# TESTS
# ============================================================

if(OUTLAND_BUILD_TESTS)

    enable_testing()

    add_executable(
        outland_tests

        tests/test_main.cpp

        tests/unit/engine/test_game_time.cpp
        tests/unit/player/test_player.cpp
        tests/unit/combat/test_health.cpp
        tests/unit/world/test_world.cpp
    )

    target_link_libraries(
        outland_tests
        PRIVATE
            outland_core
    )

    target_include_directories(
        outland_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/tests
    )

    target_compile_options(
        outland_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    add_test(
        NAME outland_core_tests
        COMMAND outland_tests
    )

endif()

message(STATUS "================================")
message(STATUS "OUTLAND ${PROJECT_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${OUTLAND_BUILD_TESTS}")
message(STATUS "raylib: ${RAYLIB_VERSION}")
message(STATUS "================================")
