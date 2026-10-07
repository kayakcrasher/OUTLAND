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

option(
    OUTLAND_DEV_TOOLS
    "Build OUTLAND internal development and Creator tools"
    OFF
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

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_command(
        OUTPUT "${PROJECT_SOURCE_DIR}/assets/verda/starter/manifest.json"
               "${PROJECT_SOURCE_DIR}/assets/audio/environment/verda_wind.wav"
               "${PROJECT_SOURCE_DIR}/assets/audio/environment/gunshot_rifle.wav"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_verda_kit.py"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_verda_audio.py"
        DEPENDS tools/generate_verda_kit.py tools/verda_preview.html tools/generate_verda_audio.py
        COMMENT "Generating original Verda environment assets and audio"
        VERBATIM
    )
    add_custom_target(verda_assets DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/verda/starter/manifest.json"
        "${PROJECT_SOURCE_DIR}/assets/audio/environment/verda_wind.wav"
        "${PROJECT_SOURCE_DIR}/assets/audio/environment/gunshot_rifle.wav"
    )
elseif(NOT EXISTS "${PROJECT_SOURCE_DIR}/assets/audio/environment/verda_wind.wav")
    message(FATAL_ERROR "Install Python 3 to generate Verda assets (Termux: pkg install python).")
endif()

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
    src/engine/audio/EnvironmentAudio.cpp
    src/game/combat/CombatWorld.cpp
    src/game/combat/WeaponSystem.cpp
    src/game/combat/CombatRenderer.cpp
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

if(OUTLAND_DEV_TOOLS)
    target_sources(
        outland_render
        PRIVATE
            src/creator/CreatorController.cpp
    )

    target_compile_definitions(
        outland_render
        PUBLIC
            OUTLAND_DEV_TOOLS=1
    )

    target_compile_definitions(
        outland_core
        PUBLIC
            OUTLAND_DEV_TOOLS=1
    )

    message(
        STATUS
        "OUTLAND DEV tools enabled"
    )
endif()

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
if(TARGET verda_assets)
    add_dependencies(outland verda_assets)
endif()

target_link_libraries(
    outland
    PRIVATE
        outland_core
        outland_render
)

# Runtime assets live beside the executable, independent of the launch directory.
add_custom_command(TARGET outland POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:outland>/assets"
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

    # Record draw/audio calls to verify environment behavior without a display.
    add_executable(outland_environment_tests
        tests/integration/world/test_verda_environment.cpp
        src/world/VerdaRegion.cpp
        src/world/assets/VerdanArchitecture.cpp
        src/world/foliage/FoliageSystem.cpp
        src/world/physics/WorldCollision.cpp
        src/world/terrain/TerrainHeight.cpp
        src/engine/audio/EnvironmentAudio.cpp
    )
    target_include_directories(outland_environment_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS}
    )
    target_compile_options(outland_environment_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(outland_environment_tests PRIVATE -UNDEBUG)
    add_test(NAME outland_environment_tests COMMAND outland_environment_tests)
    add_executable(outland_combat_tests
        tests/integration/world/test_combat.cpp
        src/game/combat/CombatWorld.cpp
        src/game/combat/WeaponSystem.cpp
        src/world/terrain/TerrainHeight.cpp
    )
    target_include_directories(outland_combat_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_combat_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(outland_combat_tests PRIVATE -UNDEBUG)
    add_test(NAME outland_combat_tests COMMAND outland_combat_tests)

    add_executable(outland_combat_input_tests tests/integration/world/test_combat_input.cpp src/input/InputSystem.cpp)
    target_include_directories(outland_combat_input_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_combat_input_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_combat_input_tests COMMAND outland_combat_input_tests)

    if(Python3_Interpreter_FOUND)
        add_dependencies(outland_environment_tests verda_assets)
        add_test(NAME outland_asset_validation
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/validate_verda_kit.py"
        )
    endif()

endif()

message(STATUS "================================")
message(STATUS "OUTLAND ${PROJECT_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${OUTLAND_BUILD_TESTS}")
message(STATUS "raylib: ${RAYLIB_VERSION}")
message(STATUS "================================")
