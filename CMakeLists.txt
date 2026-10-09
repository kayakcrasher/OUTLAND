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

    src/assets/ModelCache.cpp
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
    src/characters/CharacterRegistry.cpp
    src/characters/CharacterRenderer.cpp
    src/characters/NpcSystem.cpp
    src/characters/NpcBehavior.cpp
    src/characters/AnimationController.cpp
    src/characters/SkeletalGait.cpp
    src/world/physics/WorldCollision.cpp
    src/world/assets/VerdanArchitecture.cpp
    src/world/assets/VerdanProps.cpp
    src/world/terrain/TerrainHeight.cpp
    src/world/terrain/TerrainChunk.cpp
    src/world/terrain/TerrainWorld.cpp

    src/creator/CreatorMapIO.cpp
    src/input/InputSystem.cpp
    src/input/TouchHUD.cpp
)

if(Python3_Interpreter_FOUND)
    file(GLOB_RECURSE OUTLAND_URBAN_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/urban/*.glb")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/UrbanAssetCatalog.hpp"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_urban_catalog.py"
        DEPENDS tools/generate_urban_catalog.py tools/gltf_import.py ${OUTLAND_URBAN_MODELS}
        VERBATIM)
    add_custom_target(outland_urban_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/UrbanAssetCatalog.hpp")
    add_dependencies(outland_render outland_urban_catalog)
    file(GLOB OUTLAND_SURVIVAL_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/survival/runtime/*.glb")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/SurvivalAssetCatalog.hpp"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_survival_catalog.py"
        DEPENDS tools/generate_survival_catalog.py tools/gltf_import.py assets/verda/survival/runtime_manifest.json ${OUTLAND_SURVIVAL_MODELS} VERBATIM)
    add_custom_target(outland_survival_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/SurvivalAssetCatalog.hpp")
    add_dependencies(outland_render outland_survival_catalog)
    file(GLOB OUTLAND_STARTER_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/starter/*.obj")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/StarterAssetCatalog.hpp"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_starter_catalog.py"
        DEPENDS tools/generate_starter_catalog.py ${OUTLAND_STARTER_MODELS} VERBATIM)
    add_custom_target(outland_starter_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/StarterAssetCatalog.hpp")
    add_dependencies(outland_render outland_starter_catalog)
    file(GLOB_RECURSE OUTLAND_CHARACTER_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/characters/*.glb")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/CharacterAssetCatalog.hpp"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_character_catalog.py"
        DEPENDS tools/generate_character_catalog.py tools/gltf_import.py tools/character_manifest.py
            assets/verda/characters/character_manifest.tsv ${OUTLAND_CHARACTER_MODELS}
        VERBATIM)
    add_custom_target(outland_character_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/CharacterAssetCatalog.hpp")
    add_dependencies(outland_render outland_character_catalog)
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/characters/AnimationAssetCatalog.hpp"
        "${PROJECT_SOURCE_DIR}/docs/character-rig-compatibility.json" "${PROJECT_SOURCE_DIR}/docs/character-rig-compatibility.md"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_character_rigs.py"
        DEPENDS tools/audit_character_rigs.py tools/character_manifest.py tools/gltf_import.py
            assets/verda/characters/character_manifest.tsv ${OUTLAND_CHARACTER_MODELS}
        VERBATIM)
    add_custom_target(outland_character_rigs DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/characters/AnimationAssetCatalog.hpp")
    add_dependencies(outland_render outland_character_rigs)
endif()

if(OUTLAND_DEV_TOOLS)
    target_sources(
        outland_render
        PRIVATE
            src/creator/CreatorController.cpp
            src/creator/CreatorSession.cpp
            src/creator/CreatorTouchUI.cpp
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
        ${RAYLIB_INCLUDE_DIRS}
)

target_link_directories(
    outland_render
    PUBLIC
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps" "$<TARGET_FILE_DIR:outland>/maps"
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

    add_executable(outland_model_cache_tests tests/integration/assets/test_model_cache.cpp src/assets/ModelCache.cpp)
    target_include_directories(outland_model_cache_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_model_cache_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_model_cache_tests COMMAND outland_model_cache_tests)

    add_executable(outland_character_renderer_tests tests/integration/assets/test_character_renderer.cpp
        src/characters/CharacterRegistry.cpp src/characters/CharacterRenderer.cpp src/characters/AnimationController.cpp src/characters/SkeletalGait.cpp src/assets/ModelCache.cpp)
    target_include_directories(outland_character_renderer_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_character_renderer_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_character_renderer_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_character_renderer_tests COMMAND outland_character_renderer_tests)

    add_executable(outland_character_tests tests/integration/assets/test_characters.cpp)
    target_link_libraries(outland_character_tests PRIVATE outland_render)
    target_compile_options(outland_character_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_character_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_character_tests COMMAND outland_character_tests)

    add_executable(outland_npc_ai_tests tests/integration/assets/test_npc_ai.cpp)
    target_link_libraries(outland_npc_ai_tests PRIVATE outland_render)
    target_compile_options(outland_npc_ai_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_npc_ai_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_npc_ai_tests COMMAND outland_npc_ai_tests)

    add_executable(outland_animation_asset_tests tests/integration/assets/test_animation_assets.cpp)
    target_link_libraries(outland_animation_asset_tests PRIVATE outland_render)
    target_compile_options(outland_animation_asset_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_animation_asset_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_animation_asset_tests COMMAND outland_animation_asset_tests)

    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_survival_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_survival_catalog.py" --check)
        add_test(NAME outland_starter_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_starter_catalog.py" --check)
    endif()
    add_executable(outland_skeletal_gait_tests tests/integration/assets/test_skeletal_gait.cpp src/characters/SkeletalGait.cpp)
    target_include_directories(outland_skeletal_gait_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_skeletal_gait_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_skeletal_gait_tests COMMAND outland_skeletal_gait_tests)

    add_executable(outland_animation_tests tests/integration/assets/test_animation_controller.cpp src/characters/AnimationController.cpp)
    target_include_directories(outland_animation_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_animation_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_animation_tests COMMAND outland_animation_tests)

    add_executable(outland_runtime_asset_tests tests/integration/assets/test_runtime_assets.cpp)
    target_link_libraries(outland_runtime_asset_tests PRIVATE outland_render)
    target_compile_options(outland_runtime_asset_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_runtime_asset_tests COMMAND outland_runtime_asset_tests)

    # Manual smoke target requires a display; keep headless CTest independent.
    add_executable(outland_asset_model_smoke tests/integration/assets/smoke_imported_models.cpp)
    target_link_libraries(outland_asset_model_smoke PRIVATE outland_render)
    target_compile_options(outland_asset_model_smoke PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)

    if(OUTLAND_DEV_TOOLS)
        add_executable(outland_creator_session_tests tests/integration/creator/test_creator_session.cpp)
        target_link_libraries(outland_creator_session_tests PRIVATE outland_render)
        target_compile_options(outland_creator_session_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        add_test(NAME outland_creator_session_tests COMMAND outland_creator_session_tests)

        add_executable(outland_creator_touch_tests tests/integration/creator/test_creator_touch_ui.cpp)
        target_link_libraries(outland_creator_touch_tests PRIVATE outland_render)
        target_compile_options(outland_creator_touch_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_creator_touch_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        # Redirect input polling while retaining real raylib geometry/controller code.
        target_link_options(outland_creator_touch_tests PRIVATE
            -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPointId
            -Wl,--wrap=GetTouchPosition -Wl,--wrap=GetMousePosition
            -Wl,--wrap=IsMouseButtonPressed -Wl,--wrap=IsMouseButtonDown
            -Wl,--wrap=IsKeyPressed -Wl,--wrap=IsKeyDown -Wl,--wrap=GetCharPressed)
        add_test(NAME outland_creator_touch_tests COMMAND outland_creator_touch_tests)

        add_executable(
            outland_creator_map_tests
            tests/integration/creator/test_creator_map_io.cpp
        )

        target_link_libraries(
            outland_creator_map_tests
            PRIVATE
                outland_render
        )

        target_compile_options(
            outland_creator_map_tests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -UNDEBUG
        )

        add_test(
            NAME outland_creator_map_tests
            COMMAND outland_creator_map_tests
        )
    endif()

    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_urban_asset_completeness
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_downloaded_assets.py" --pack urban --strict-textures)
        add_test(NAME outland_character_rig_audit
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_character_rigs.py" --check)
        add_test(NAME outland_character_asset_completeness
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_character_assets.py")
        add_test(NAME outland_character_catalog_validation
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_character_catalog.py" --check)
        add_test(NAME outland_urban_catalog_validation
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_urban_catalog.py" --check)
        add_test(NAME outland_downloaded_asset_geometry
            COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_downloaded_assets.py")
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
