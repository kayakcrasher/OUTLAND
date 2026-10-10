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
    raylib>=6.0
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

    src/game/vehicles/VehicleRegistry.cpp
    src/game/vehicles/VehicleSystem.cpp
    src/game/vehicles/VehicleSave.cpp
    src/game/vehicles/VehicleRenderer.cpp
    src/game/vehicles/VehicleAudio.cpp
    src/assets/ModelCache.cpp
    src/engine/render/Renderer.cpp
    src/engine/audio/EnvironmentAudio.cpp
    src/game/combat/CombatWorld.cpp
    src/game/combat/WeaponSystem.cpp
    src/game/combat/CombatRenderer.cpp
    src/game/inventory/ItemRegistry.cpp
    src/game/inventory/Inventory.cpp
    src/game/inventory/LootWorld.cpp
    src/game/inventory/LootSave.cpp
    src/game/inventory/LootSession.cpp
    src/game/inventory/LootUI.cpp
    src/world/VerdaRegion.cpp
    src/dev/DevLab.cpp
    src/game/HomeScreen.cpp
    src/game/GameMode.cpp
    src/world/foliage/FoliageSystem.cpp
    src/characters/CharacterRegistry.cpp
    src/characters/CharacterRenderer.cpp
    src/characters/NpcSystem.cpp
    src/characters/NpcBehavior.cpp
    src/game/life/LifeTypes.cpp
    src/game/life/LifePopulation.cpp
    src/game/life/LifeSimulation.cpp
    src/game/ai/CombatBot.cpp
    src/game/ai/BattleRoyaleBots.cpp
    src/characters/AnimationController.cpp
    src/characters/SkeletalGait.cpp
    src/world/physics/WorldCollision.cpp
    src/world/physics/MeshCollision.cpp
    src/world/assets/VerdanArchitecture.cpp
    src/world/assets/VerdanProps.cpp
    src/world/terrain/TerrainHeight.cpp
    src/world/terrain/TerrainChunk.cpp
    src/world/terrain/TerrainWorld.cpp

    src/creator/CreatorMapIO.cpp
    src/input/InputSystem.cpp
        src/input/PointerEvents.cpp
    src/input/TouchHUD.cpp
)

if(Python3_Interpreter_FOUND)
    file(GLOB_RECURSE OUTLAND_VEHICLE_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/ggbot/*.glb")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/VehicleAssetCatalog.hpp"
        "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/vehicle_manifest.tsv"
        "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/runtime/hatchback_body.glb"
        "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/runtime/hatchback_wheel.glb"
        "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/runtime/hatchback_preview.glb"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/prepare_vehicle_assets.py"
        DEPENDS tools/prepare_vehicle_assets.py assets/verda/vehicles/hatchback/compact_classic.glb ${OUTLAND_VEHICLE_MODELS}
        VERBATIM)
    add_custom_target(outland_vehicle_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/VehicleAssetCatalog.hpp")
    add_dependencies(outland_render outland_vehicle_catalog)
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

    # Generates maps/verda_world.map from the default towns and the Creator catalog.
    add_executable(outland_build_world src/dev/BuildVerdaWorld.cpp src/dev/VerdaWorldKit.cpp)
    target_link_libraries(outland_build_world PRIVATE outland_render)
    target_compile_options(outland_build_world PRIVATE -Wall -Wextra -Wpedantic)

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
        ${CMAKE_DL_LIBS}
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
add_custom_target(outland_vehicle_data
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:outland>/assets/verda/vehicles"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/assets/verda/vehicles/vehicle_manifest.tsv" "$<TARGET_FILE_DIR:outland>/assets/verda/vehicles/vehicle_manifest.tsv")
add_dependencies(outland outland_vehicle_data)
if(TARGET outland_vehicle_catalog)
    add_dependencies(outland_vehicle_data outland_vehicle_catalog)
endif()


target_link_libraries(
    outland
    PRIVATE
        outland_core
        outland_render
)

# Keep tunable loot metadata current even when only TSV/map data changed.
add_custom_target(outland_loot_data
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:outland>/assets/verda/survival/gameplay"
        "$<TARGET_FILE_DIR:outland>/maps"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/verda/survival/gameplay/items.tsv"
        "${PROJECT_SOURCE_DIR}/assets/verda/survival/gameplay/loot_tables.tsv"
        "$<TARGET_FILE_DIR:outland>/assets/verda/survival/gameplay"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:outland>/maps"
    VERBATIM
)
add_dependencies(outland outland_loot_data)

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
        src/world/physics/MeshCollision.cpp
        src/world/terrain/TerrainHeight.cpp
        src/engine/audio/EnvironmentAudio.cpp
    )
    target_include_directories(outland_environment_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS}
    )
    target_compile_options(outland_environment_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(outland_environment_tests PRIVATE -UNDEBUG)
    add_test(NAME outland_environment_tests COMMAND outland_environment_tests)
    add_executable(outland_verda_layout_tests tests/integration/world/test_verda_layout.cpp)
    target_link_libraries(outland_verda_layout_tests PRIVATE outland_render)
    target_compile_options(outland_verda_layout_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_link_options(outland_verda_layout_tests PRIVATE
        -Wl,--wrap=UploadMesh -Wl,--wrap=LoadModelFromMesh -Wl,--wrap=UnloadModel
        -Wl,--wrap=DrawModel -Wl,--wrap=DrawPlane -Wl,--wrap=IsKeyPressed)
    add_test(NAME outland_verda_layout_tests COMMAND outland_verda_layout_tests)
    add_executable(outland_combat_tests
        tests/integration/world/test_combat.cpp
        src/game/combat/CombatWorld.cpp
        src/world/physics/MeshCollision.cpp
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

    add_executable(outland_loot_ui_tests tests/integration/inventory/test_loot_ui.cpp)
    target_link_libraries(outland_loot_ui_tests PRIVATE outland_render)
    target_compile_options(outland_loot_ui_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_loot_ui_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_options(outland_loot_ui_tests PRIVATE
        -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPointId -Wl,--wrap=GetTouchPosition
        -Wl,--wrap=GetMousePosition -Wl,--wrap=IsMouseButtonPressed -Wl,--wrap=IsKeyPressed -Wl,--wrap=IsKeyDown)
    add_test(NAME outland_loot_ui_tests COMMAND outland_loot_ui_tests)

    add_executable(outland_loot_tests tests/integration/inventory/test_loot.cpp)
    target_link_libraries(outland_loot_tests PRIVATE outland_render)
    target_compile_options(outland_loot_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_loot_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_loot_tests COMMAND outland_loot_tests)

    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_vehicle_asset_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/prepare_vehicle_assets.py" --check)
    endif()
    add_executable(outland_vehicle_tests tests/integration/vehicles/test_vehicles.cpp)
    target_link_libraries(outland_vehicle_tests PRIVATE outland_render)
    target_compile_options(outland_vehicle_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_vehicle_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_vehicle_tests COMMAND outland_vehicle_tests)
    add_executable(outland_vehicle_audio_tests tests/integration/vehicles/test_vehicle_audio.cpp)
    target_link_libraries(outland_vehicle_audio_tests PRIVATE outland_render)
    target_compile_options(outland_vehicle_audio_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_vehicle_audio_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_options(outland_vehicle_audio_tests PRIVATE
        -Wl,--wrap=IsAudioDeviceReady -Wl,--wrap=FileExists -Wl,--wrap=GetApplicationDirectory
        -Wl,--wrap=LoadMusicStream -Wl,--wrap=LoadSound -Wl,--wrap=UnloadMusicStream -Wl,--wrap=UnloadSound
        -Wl,--wrap=SetMusicVolume -Wl,--wrap=SetSoundVolume -Wl,--wrap=SetMusicPitch
        -Wl,--wrap=PlayMusicStream -Wl,--wrap=UpdateMusicStream -Wl,--wrap=StopMusicStream
        -Wl,--wrap=IsSoundPlaying -Wl,--wrap=PlaySound -Wl,--wrap=StopSound)
    add_test(NAME outland_vehicle_audio_tests COMMAND outland_vehicle_audio_tests)
    add_executable(outland_raylib6_pose_tests tests/integration/assets/test_raylib6_pose.cpp)
    target_link_libraries(outland_raylib6_pose_tests PRIVATE outland_render)
    target_compile_options(outland_raylib6_pose_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_raylib6_pose_tests COMMAND outland_raylib6_pose_tests)

    add_executable(outland_model_cache_tests tests/integration/assets/test_model_cache.cpp src/assets/ModelCache.cpp)
    target_include_directories(outland_model_cache_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_model_cache_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_model_cache_tests COMMAND outland_model_cache_tests)

    add_executable(outland_character_renderer_tests tests/integration/assets/test_character_renderer.cpp
        src/characters/CharacterRegistry.cpp src/characters/CharacterRenderer.cpp src/characters/AnimationController.cpp src/characters/SkeletalGait.cpp src/assets/ModelCache.cpp src/world/physics/MeshCollision.cpp)
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

    add_executable(outland_mesh_collision_tests tests/integration/world/test_mesh_collision.cpp)
    target_link_libraries(outland_mesh_collision_tests PRIVATE outland_render)
    target_compile_options(outland_mesh_collision_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_mesh_collision_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_mesh_collision_tests COMMAND outland_mesh_collision_tests)

    add_executable(outland_life_tests tests/integration/life/test_life_simulation.cpp)
    target_link_libraries(outland_life_tests PRIVATE outland_render)
    target_compile_options(outland_life_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_life_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_life_tests COMMAND outland_life_tests)

    add_executable(outland_combat_bot_tests tests/integration/ai/test_combat_bots.cpp)
    target_link_libraries(outland_combat_bot_tests PRIVATE outland_render)
    target_compile_options(outland_combat_bot_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_combat_bot_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_combat_bot_tests COMMAND outland_combat_bot_tests)

    add_executable(outland_animation_asset_tests tests/integration/assets/test_animation_assets.cpp)
    target_link_libraries(outland_animation_asset_tests PRIVATE outland_render)
    target_compile_options(outland_animation_asset_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_animation_asset_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_animation_asset_tests COMMAND outland_animation_asset_tests)

    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_loot_asset_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_loot_assets.py")
        add_test(NAME outland_survival_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_survival_catalog.py" --check)
        add_test(NAME outland_city_kit_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_city_kit.py" --check)
        add_test(NAME outland_kit_slab_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_kit_slabs.py" --check)
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
        add_executable(outland_world_kit_tests tests/integration/world/test_world_kit.cpp src/dev/VerdaWorldKit.cpp)
        target_link_libraries(outland_world_kit_tests PRIVATE outland_render)
        target_compile_options(outland_world_kit_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_world_kit_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        add_test(NAME outland_world_kit_tests COMMAND outland_world_kit_tests)

        add_executable(outland_dev_travel_tests tests/integration/vehicles/test_dev_travel.cpp)
        target_link_libraries(outland_dev_travel_tests PRIVATE outland_render)
        target_compile_options(outland_dev_travel_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_link_options(outland_dev_travel_tests PRIVATE
            -Wl,--wrap=IsKeyPressed -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPointId
            -Wl,--wrap=GetTouchPosition -Wl,--wrap=IsWindowFocused -Wl,--wrap=IsMouseButtonPressed -Wl,--wrap=GetMousePosition)
        add_test(NAME outland_dev_travel_tests COMMAND outland_dev_travel_tests)
        add_executable(outland_creator_vehicle_tests tests/integration/vehicles/test_creator_vehicles.cpp)
        target_link_libraries(outland_creator_vehicle_tests PRIVATE outland_render)
        target_compile_options(outland_creator_vehicle_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_creator_vehicle_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        add_test(NAME outland_creator_vehicle_tests COMMAND outland_creator_vehicle_tests)
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
            -Wl,--wrap=IsKeyPressed -Wl,--wrap=IsKeyDown -Wl,--wrap=GetCharPressed -Wl,--wrap=IsWindowFocused)
        add_test(NAME outland_creator_touch_tests COMMAND outland_creator_touch_tests)

        if(UNIX)
            add_executable(outland_creative_builder_tests tests/integration/creator/test_creative_builder.cpp)
            target_link_libraries(outland_creative_builder_tests PRIVATE outland_render)
            target_compile_options(outland_creative_builder_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
            target_link_options(outland_creative_builder_tests PRIVATE
                -Wl,--wrap=dlsym -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPointId
                -Wl,--wrap=GetTouchPosition -Wl,--wrap=GetMousePosition -Wl,--wrap=IsMouseButtonPressed
                -Wl,--wrap=IsMouseButtonDown -Wl,--wrap=IsKeyPressed -Wl,--wrap=IsKeyDown
                -Wl,--wrap=GetCharPressed -Wl,--wrap=GetKeyPressed -Wl,--wrap=IsWindowFocused)
            add_test(NAME outland_creative_builder_tests COMMAND outland_creative_builder_tests)
        endif()

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
