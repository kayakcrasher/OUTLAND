cmake_minimum_required(VERSION 3.20)

project(
    Outland
    VERSION 0.2.0
    DESCRIPTION "Offline 3D open-world combat sandbox"
    LANGUAGES C CXX
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

# Cross builds must never discover the host's raylib through pkg-config.
if(ANDROID OR EMSCRIPTEN)
    if(CMAKE_VERSION VERSION_LESS 3.25)
        message(FATAL_ERROR "Android/Web raylib 6 builds require CMake 3.25 or newer.")
    endif()
    if(OUTLAND_DEV_TOOLS OR OUTLAND_BUILD_TESTS)
        message(FATAL_ERROR "Platform releases require OUTLAND_DEV_TOOLS=OFF and OUTLAND_BUILD_TESTS=OFF; run shared tests on desktop.")
    endif()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    if(ANDROID)
        set(PLATFORM Android CACHE STRING "" FORCE)
        set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    else()
        set(PLATFORM Web CACHE STRING "" FORCE)
        set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    endif()
    # Immutable upstream raylib 6.0 release; FETCHCONTENT_SOURCE_DIR_RAYLIB
    # can point at an existing checkout for offline builds.
    FetchContent_Declare(raylib
        URL https://codeload.github.com/raysan5/raylib/tar.gz/dbc56a87da87d973a9c5baa4e7438a9d20121d28
        URL_HASH SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126)
    FetchContent_MakeAvailable(raylib)
    if(EMSCRIPTEN)
        # raylib exports only ccall in a transitive link option. Emscripten uses
        # the last export list, so that option would discard our PLAY entrypoint.
        get_target_property(raylib_web_link_options raylib INTERFACE_LINK_OPTIONS)
        list(FILTER raylib_web_link_options EXCLUDE REGEX "^-sEXPORTED_RUNTIME_METHODS=")
        set_property(TARGET raylib PROPERTY INTERFACE_LINK_OPTIONS "${raylib_web_link_options}")
    endif()
    if(ANDROID)
        # raylib's default Android link set only includes GLESv2, even when ES3 is selected.
        target_link_libraries(raylib PRIVATE GLESv3)
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/patch_raylib_platform.py" "${raylib_SOURCE_DIR}" "${PLATFORM}" COMMAND_ERROR_IS_FATAL ANY)
    set(RAYLIB_LIBRARIES raylib)
    set(RAYLIB_INCLUDE_DIRS "${raylib_SOURCE_DIR}/src")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED raylib>=6.0)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_command(
        OUTPUT "${PROJECT_SOURCE_DIR}/assets/verda/starter/manifest.json"
               "${PROJECT_SOURCE_DIR}/assets/audio/environment/verda_wind.wav"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_verda_kit.py"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_verda_audio.py"
        DEPENDS tools/generate_verda_kit.py tools/verda_preview.html tools/generate_verda_audio.py
        COMMENT "Generating original Verda environment assets and audio"
        VERBATIM
    )
    add_custom_target(verda_assets DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/verda/starter/manifest.json"
        "${PROJECT_SOURCE_DIR}/assets/audio/environment/verda_wind.wav"
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
    src/platform/Runtime.cpp
    src/platform/Haptics.cpp
    src/assets/ModelCache.cpp
    src/engine/render/Renderer.cpp
    src/engine/render/GeometryRecorder.cpp
    src/engine/render/StaticWorld.cpp
    src/engine/audio/EnvironmentAudio.cpp
    src/engine/audio/GunAudio.cpp
    src/game/combat/CombatWorld.cpp
    src/game/combat/WeaponSystem.cpp
    src/game/combat/CombatFeedback.cpp
    src/game/combat/Grenades.cpp
    src/game/combat/Sights.cpp
    src/game/build/PlayerBuilds.cpp
    src/game/build/BuildMode.cpp
    src/game/hud/Instruments.cpp
    src/game/combat/CombatRenderer.cpp
    src/game/inventory/ItemRegistry.cpp
    src/game/inventory/Inventory.cpp
    src/game/inventory/LootWorld.cpp
    src/game/inventory/LootSave.cpp
    src/game/inventory/LootSession.cpp
    src/game/inventory/LootUI.cpp
    src/game/economy/Economy.cpp
    src/game/economy/Penthouse.cpp
    src/game/ModeSession.cpp
    src/game/PlayUI.cpp
    src/game/MapScreen.cpp
    src/game/Settings.cpp
    src/game/TouchLayoutEditor.cpp
    src/game/PauseMenu.cpp
    src/game/feel/Haptics.cpp
    src/game/transit/BusLine.cpp
    src/world/VerdaRegion.cpp
    src/world/Zones.cpp
    src/game/HomeScreen.cpp
    src/game/GameMode.cpp
    src/world/foliage/FoliageSystem.cpp
    src/characters/CharacterRegistry.cpp
    src/characters/CharacterRenderer.cpp
    src/characters/NpcSystem.cpp
    src/characters/NpcBehavior.cpp
    src/game/life/LifeTypes.cpp
    src/game/life/LifePopulation.cpp
    src/game/life/WalkingMan.cpp
    src/game/ranch/Ranches.cpp
    src/game/freight/Freight.cpp
    src/game/freight/Trucking.cpp
    src/game/campaign/Campaign.cpp
    src/game/radio/Radio.cpp
    src/game/radio/RadioAudio.cpp
    src/game/life/LifeSimulation.cpp
    src/game/life/LifeTraffic.cpp
    src/game/zombies/ZombieEcology.cpp
    src/game/law/Justice.cpp
    src/game/law/PrisonYard.cpp
    src/game/culture/Esperanto.cpp
    src/game/culture/Signage.cpp
    src/game/culture/SignRenderer.cpp
    src/game/sound/SoundBus.cpp
    src/game/ai/CombatBot.cpp
    src/game/ai/BattleRoyaleBots.cpp
    src/game/ai/BotDrivers.cpp
    src/game/mafia/Mafio.cpp
    src/game/mafia/Territory.cpp
    src/game/mafia/GangLogos.cpp
    src/game/career/Career.cpp
    src/game/career/Crew.cpp
    src/game/career/Backup.cpp
    src/game/career/TrainingCamp.cpp
    src/game/career/CampRenderer.cpp
    src/game/services/EmergencyServices.cpp
    src/game/services/HighwayPatrol.cpp
    src/game/interiors/Furnishing.cpp
    src/game/wardrobe/Wardrobe.cpp
    src/game/wardrobe/StoreUI.cpp
    src/game/economy/Homes.cpp
    src/game/mafia/MafioRenderer.cpp
    src/game/casino/Poker.cpp
    src/game/casino/PokerTable.cpp
    src/game/ZoneWall.cpp
    src/game/aircraft/HelicopterDrop.cpp
    src/game/aircraft/DropRenderer.cpp
    src/characters/AnimationController.cpp
    src/characters/SkeletalGait.cpp
    src/world/physics/WorldCollision.cpp
    src/world/physics/MeshCollision.cpp
    src/world/navigation/NavGrid.cpp
    src/world/roads/RoadGraph.cpp
    src/world/roads/HighwayNetwork.cpp
    src/engine/render/HighwayRender.cpp
    src/net/LanSocket.cpp
    src/net/LanProtocol.cpp
    src/net/LanSession.cpp
    src/game/LanScreen.cpp
    src/game/lan/LanPlay.cpp
    src/world/sky/DayNight.cpp
    src/world/assets/VerdanArchitecture.cpp
    src/world/assets/VerdanProps.cpp
    src/world/terrain/TerrainHeight.cpp
    src/world/Highways.cpp
    src/world/terrain/TerrainChunk.cpp
    src/world/terrain/TerrainWorld.cpp

    src/creator/CreatorMapIO.cpp
    src/input/InputSystem.cpp
    src/input/TouchCustom.cpp
    src/input/Bindings.cpp
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
    file(GLOB OUTLAND_INTERIOR_MODELS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/verda/interiors/props/*.glb")
    add_custom_command(OUTPUT "${PROJECT_SOURCE_DIR}/include/outland/creator/EconomyAssetCatalog.hpp"
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_interior_catalog.py"
        DEPENDS tools/generate_interior_catalog.py tools/gltf_import.py
            assets/verda/survival/gameplay/items.tsv ${OUTLAND_INTERIOR_MODELS} VERBATIM)
    add_custom_target(outland_interior_catalog DEPENDS "${PROJECT_SOURCE_DIR}/include/outland/creator/EconomyAssetCatalog.hpp")
    add_dependencies(outland_render outland_interior_catalog)
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
            src/dev/DevLab.cpp
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

if(ANDROID)
    add_library(outland SHARED src/main.cpp)
    target_include_directories(outland_render PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
    target_link_options(outland PRIVATE "-Wl,-u,ANativeActivity_onCreate" "-Wl,-z,max-page-size=16384")
    target_link_libraries(outland PRIVATE android log)
else()
    add_executable(outland src/main.cpp)
endif()
if(EMSCRIPTEN)
    set_target_properties(outland PROPERTIES SUFFIX ".html")
    set(OUTLAND_RUNTIME_ASSETS "${CMAKE_BINARY_DIR}/runtime" CACHE PATH "Staged production assets")
    add_custom_target(outland_platform_assets
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/stage_runtime_assets.py" "${OUTLAND_RUNTIME_ASSETS}"
        VERBATIM)
    foreach(generator verda_assets outland_vehicle_catalog outland_survival_catalog outland_character_catalog outland_interior_catalog outland_starter_catalog)
        if(TARGET ${generator})
            add_dependencies(outland_platform_assets ${generator})
        endif()
    endforeach()
    add_dependencies(outland outland_platform_assets)
    set_property(TARGET outland APPEND PROPERTY LINK_DEPENDS "${OUTLAND_RUNTIME_ASSETS}/runtime.manifest" "${PROJECT_SOURCE_DIR}/platform/web/shell.html" "${PROJECT_SOURCE_DIR}/platform/web/storage.js")
    target_compile_options(outland_render PRIVATE -O2)
    target_link_options(outland PRIVATE
        -O2 -sASYNCIFY=1 -sASYNCIFY_IGNORE_INDIRECT=1 -sASYNCIFY_STACK_SIZE=262144
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=67108864 -sMAXIMUM_MEMORY=1073741824
        -sSTACK_SIZE=2097152 -sFORCE_FILESYSTEM=1
        "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','ccall']" -lidbfs.js
        "--shell-file=${PROJECT_SOURCE_DIR}/platform/web/shell.html"
        "--pre-js=${PROJECT_SOURCE_DIR}/platform/web/storage.js"
        "--preload-file=${OUTLAND_RUNTIME_ASSETS}/assets@/assets"
        "--preload-file=${OUTLAND_RUNTIME_ASSETS}/maps@/maps")
endif()
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
        "$<TARGET_FILE_DIR:outland>/assets/verda/economy"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/verda/survival/gameplay/items.tsv"
        "${PROJECT_SOURCE_DIR}/assets/verda/survival/gameplay/loot_tables.tsv"
        "$<TARGET_FILE_DIR:outland>/assets/verda/survival/gameplay"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/verda/economy" "$<TARGET_FILE_DIR:outland>/assets/verda/economy"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps"
        "$<TARGET_FILE_DIR:outland>/maps"
    VERBATIM
)
add_dependencies(outland outland_loot_data)

# Runtime assets live beside the executable, independent of the launch directory.
if(NOT ANDROID AND NOT EMSCRIPTEN)
add_custom_command(TARGET outland POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:outland>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/maps" "$<TARGET_FILE_DIR:outland>/maps"
)

# desktop copying must not overwrite preloaded/extracted platform assets.
endif()

# ============================================================
# TESTS
# ============================================================

if(OUTLAND_BUILD_TESTS)
    add_executable(outland_render_performance_tests tests/integration/world/test_render_performance.cpp)
    target_link_libraries(outland_render_performance_tests PRIVATE outland_render)
    target_compile_options(outland_render_performance_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_render_performance_tests COMMAND outland_render_performance_tests)
    add_executable(outland_render_benchmark tools/benchmark_world_render.cpp)
    target_link_libraries(outland_render_benchmark PRIVATE outland_render)

    enable_testing()
    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_android_runtime_tests COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/platform/test_android_runtime.py")
        add_test(NAME outland_web_contact_tests COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/platform/test_web_contacts.py")
        add_test(NAME outland_platform_package_tests COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/platform/test_runtime_package.py")
    endif()

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
        src/world/roads/HighwayNetwork.cpp
        src/world/physics/MeshCollision.cpp
        src/world/terrain/TerrainHeight.cpp
        src/world/Highways.cpp
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
    if(NOT OUTLAND_DEV_TOOLS)
        # Layout tests exercise DEV destinations, without shipping DevLab in releases.
        target_sources(outland_verda_layout_tests PRIVATE src/dev/DevLab.cpp)
    endif()
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
        src/world/Highways.cpp
    )
    target_include_directories(outland_combat_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_combat_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(outland_combat_tests PRIVATE -UNDEBUG)
    add_test(NAME outland_combat_tests COMMAND outland_combat_tests)

    add_executable(outland_grenade_feedback_tests tests/unit/combat/test_grenades_feedback.cpp
        src/game/combat/CombatFeedback.cpp src/game/combat/Grenades.cpp)
    target_include_directories(outland_grenade_feedback_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_grenade_feedback_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_grenade_feedback_tests COMMAND outland_grenade_feedback_tests)

    add_executable(outland_instrument_tests tests/unit/hud/test_instruments.cpp)
    target_link_libraries(outland_instrument_tests PRIVATE outland_render)
    target_compile_options(outland_instrument_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_instrument_tests COMMAND outland_instrument_tests)

    add_executable(outland_sight_tests tests/unit/combat/test_sights.cpp src/game/combat/Sights.cpp)
    target_include_directories(outland_sight_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_options(outland_sight_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_sight_tests COMMAND outland_sight_tests)

    add_executable(outland_gun_audio_tests tests/unit/audio/test_gun_mix.cpp)
    target_link_libraries(outland_gun_audio_tests PRIVATE outland_render)
    target_compile_options(outland_gun_audio_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_gun_audio_tests COMMAND outland_gun_audio_tests)
    add_executable(outland_combat_input_tests tests/integration/world/test_combat_input.cpp src/input/InputSystem.cpp src/input/Bindings.cpp)
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

    add_executable(outland_play_ui_tests tests/integration/modes/test_play_ui.cpp)
    target_link_libraries(outland_play_ui_tests PRIVATE outland_render)
    target_compile_options(outland_play_ui_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_play_ui_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_options(outland_play_ui_tests PRIVATE
        -Wl,--wrap=GetTouchPointCount -Wl,--wrap=GetTouchPointId -Wl,--wrap=GetTouchPosition
        -Wl,--wrap=GetMousePosition -Wl,--wrap=IsMouseButtonPressed -Wl,--wrap=IsKeyPressed)
    add_test(NAME outland_play_ui_tests COMMAND outland_play_ui_tests)

    foreach(name IN ITEMS mode_sessions economy explore_flow)
        if(name STREQUAL "mode_sessions")
            set(test_source tests/integration/modes/test_mode_sessions.cpp)
        elseif(name STREQUAL "explore_flow")
            set(test_source tests/integration/modes/test_explore_flow.cpp)
        else()
            set(test_source tests/integration/inventory/test_economy.cpp)
        endif()
        add_executable(outland_${name}_tests ${test_source})
        target_link_libraries(outland_${name}_tests PRIVATE outland_render)
        target_compile_options(outland_${name}_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_${name}_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        add_test(NAME outland_${name}_tests COMMAND outland_${name}_tests)
    endforeach()

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

    add_executable(outland_sound_bus_tests tests/integration/sound/test_sound_bus.cpp)
    target_link_libraries(outland_sound_bus_tests PRIVATE outland_render)
    target_compile_options(outland_sound_bus_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_sound_bus_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_sound_bus_tests COMMAND outland_sound_bus_tests)

    add_executable(outland_day_night_tests tests/integration/sky/test_day_night.cpp)
    target_link_libraries(outland_day_night_tests PRIVATE outland_render)
    target_compile_options(outland_day_night_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_day_night_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_day_night_tests COMMAND outland_day_night_tests)

    add_executable(outland_justice_tests tests/integration/law/test_justice.cpp)
    target_link_libraries(outland_justice_tests PRIVATE outland_render)
    target_compile_options(outland_justice_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_justice_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_justice_tests COMMAND outland_justice_tests)

    add_executable(outland_signage_tests tests/integration/culture/test_signage.cpp)
    target_link_libraries(outland_signage_tests PRIVATE outland_render)
    target_compile_options(outland_signage_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_signage_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_signage_tests COMMAND outland_signage_tests)

    add_executable(outland_traffic_tests tests/integration/life/test_traffic.cpp)
    target_link_libraries(outland_traffic_tests PRIVATE outland_render)
    target_compile_options(outland_traffic_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_traffic_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_traffic_tests COMMAND outland_traffic_tests)

    add_executable(outland_player_build_tests tests/integration/world/test_player_builds.cpp)
    target_link_libraries(outland_player_build_tests PRIVATE outland_render)
    target_compile_options(outland_player_build_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_player_build_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_player_build_tests COMMAND outland_player_build_tests)

    add_executable(outland_bot_driver_tests tests/integration/ai/test_bot_drivers.cpp)
    target_link_libraries(outland_bot_driver_tests PRIVATE outland_render)
    target_compile_options(outland_bot_driver_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_bot_driver_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_bot_driver_tests COMMAND outland_bot_driver_tests)
    add_executable(outland_road_audit_tests tests/integration/world/test_road_audit.cpp)
    target_link_libraries(outland_road_audit_tests PRIVATE outland_render)
    target_compile_options(outland_road_audit_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_road_audit_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_road_audit_tests COMMAND outland_road_audit_tests)
    add_executable(outland_mafio_tests tests/integration/mafia/test_mafio.cpp)
    target_link_libraries(outland_mafio_tests PRIVATE outland_render)
    target_compile_options(outland_mafio_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_mafio_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_mafio_tests COMMAND outland_mafio_tests)
    add_executable(outland_walking_man_tests tests/integration/life/test_walking_man.cpp)
    target_link_libraries(outland_walking_man_tests PRIVATE outland_render)
    target_compile_options(outland_walking_man_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_walking_man_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_walking_man_tests COMMAND outland_walking_man_tests)
    add_executable(outland_ranch_tests tests/integration/ranch/test_ranches.cpp)
    target_link_libraries(outland_ranch_tests PRIVATE outland_render)
    target_compile_options(outland_ranch_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_ranch_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_ranch_tests COMMAND outland_ranch_tests)
    add_executable(outland_emergency_services_tests tests/integration/services/test_emergency_services.cpp)
    target_link_libraries(outland_emergency_services_tests PRIVATE outland_render)
    target_compile_options(outland_emergency_services_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_emergency_services_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_emergency_services_tests COMMAND outland_emergency_services_tests)
    add_executable(outland_poker_tests tests/unit/casino/test_poker.cpp)
    target_link_libraries(outland_poker_tests PRIVATE outland_render)
    target_compile_options(outland_poker_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_poker_tests COMMAND outland_poker_tests)

    add_executable(outland_bus_line_tests tests/integration/transit/test_bus_line.cpp)
    target_link_libraries(outland_bus_line_tests PRIVATE outland_render)
    target_compile_options(outland_bus_line_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_bus_line_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_bus_line_tests COMMAND outland_bus_line_tests)
    add_executable(outland_lan_tests tests/integration/net/test_lan.cpp)
    target_link_libraries(outland_lan_tests PRIVATE outland_render)
    target_compile_options(outland_lan_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_lan_tests COMMAND outland_lan_tests)
    add_executable(outland_campaign_tests tests/unit/campaign/test_campaign.cpp src/game/campaign/Campaign.cpp)
    target_include_directories(outland_campaign_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_campaign_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_campaign_tests COMMAND outland_campaign_tests)
    add_executable(outland_radio_tests tests/unit/radio/test_radio.cpp src/game/radio/Radio.cpp)
    target_include_directories(outland_radio_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_radio_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_radio_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_radio_tests COMMAND outland_radio_tests)
    add_executable(outland_trucking_tests tests/unit/freight/test_trucking.cpp src/game/freight/Trucking.cpp)
    target_include_directories(outland_trucking_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_trucking_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_trucking_tests COMMAND outland_trucking_tests)
    add_executable(outland_career_tests tests/unit/career/test_career.cpp src/game/career/Career.cpp)
    target_include_directories(outland_career_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_career_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_career_tests COMMAND outland_career_tests)
    add_executable(outland_territory_tests tests/unit/mafia/test_territory.cpp src/game/mafia/Territory.cpp)
    target_include_directories(outland_territory_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_territory_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_territory_tests COMMAND outland_territory_tests)
    add_executable(outland_training_camp_tests tests/unit/career/test_training_camp.cpp src/game/career/TrainingCamp.cpp)
    target_include_directories(outland_training_camp_tests PRIVATE "${PROJECT_SOURCE_DIR}/include" ${RAYLIB_INCLUDE_DIRS})
    target_compile_options(outland_training_camp_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_training_camp_tests COMMAND outland_training_camp_tests)
    add_executable(outland_highway_tests tests/integration/world/test_highways.cpp)
    target_link_libraries(outland_highway_tests PRIVATE outland_render)
    target_compile_options(outland_highway_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_highway_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_highway_tests COMMAND outland_highway_tests)
    add_executable(outland_clothing_and_furniture_tests tests/integration/interiors/test_clothing_and_furniture.cpp)
    target_link_libraries(outland_clothing_and_furniture_tests PRIVATE outland_render)
    target_compile_options(outland_clothing_and_furniture_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_clothing_and_furniture_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_clothing_and_furniture_tests COMMAND outland_clothing_and_furniture_tests)
    add_executable(outland_homes_for_sale_tests tests/integration/interiors/test_homes_for_sale.cpp)
    target_link_libraries(outland_homes_for_sale_tests PRIVATE outland_render)
    target_compile_options(outland_homes_for_sale_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_homes_for_sale_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_homes_for_sale_tests COMMAND outland_homes_for_sale_tests)
    add_executable(outland_highway_patrol_tests tests/integration/services/test_highway_patrol.cpp)
    target_link_libraries(outland_highway_patrol_tests PRIVATE outland_render)
    target_compile_options(outland_highway_patrol_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_highway_patrol_tests COMMAND outland_highway_patrol_tests)
    add_executable(outland_city_driving_tests tests/integration/vehicles/test_city_driving.cpp)
    target_link_libraries(outland_city_driving_tests PRIVATE outland_render)
    target_compile_options(outland_city_driving_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_city_driving_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_city_driving_tests COMMAND outland_city_driving_tests)
    add_executable(outland_zones_map_tests tests/integration/world/test_zones_map.cpp)
    target_link_libraries(outland_zones_map_tests PRIVATE outland_render)
    target_compile_options(outland_zones_map_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_zones_map_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_zones_map_tests COMMAND outland_zones_map_tests)
    add_executable(outland_helicopter_drop_tests tests/integration/modes/test_helicopter_drop.cpp)
    target_link_libraries(outland_helicopter_drop_tests PRIVATE outland_render)
    target_compile_options(outland_helicopter_drop_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_helicopter_drop_tests COMMAND outland_helicopter_drop_tests)
    add_executable(outland_pause_menu_tests tests/integration/world/test_pause_menu.cpp)
    target_link_libraries(outland_pause_menu_tests PRIVATE outland_render)
    target_compile_options(outland_pause_menu_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_pause_menu_tests COMMAND outland_pause_menu_tests)
    add_executable(outland_haptics_tests tests/unit/feel/test_haptics.cpp src/game/feel/Haptics.cpp src/platform/Haptics.cpp)
    target_include_directories(outland_haptics_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_options(outland_haptics_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    add_test(NAME outland_haptics_tests COMMAND outland_haptics_tests)
    add_executable(outland_zombie_ecology_tests tests/integration/zombies/test_zombie_ecology.cpp)
    target_link_libraries(outland_zombie_ecology_tests PRIVATE outland_render)
    target_compile_options(outland_zombie_ecology_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_zombie_ecology_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_zombie_ecology_tests COMMAND outland_zombie_ecology_tests)

    add_executable(outland_animation_asset_tests tests/integration/assets/test_animation_assets.cpp)
    target_link_libraries(outland_animation_asset_tests PRIVATE outland_render)
    target_compile_options(outland_animation_asset_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
    target_compile_definitions(outland_animation_asset_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME outland_animation_asset_tests COMMAND outland_animation_asset_tests)

    if(Python3_Interpreter_FOUND)
        add_test(NAME outland_interior_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_interior_catalog.py" --check)
        add_test(NAME outland_loot_asset_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/audit_loot_assets.py")
        add_test(NAME outland_survival_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_survival_catalog.py" --check)
        add_test(NAME outland_generated_vehicle_models COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_truck_model.py" --check)
        add_test(NAME outland_generated_emergency_vehicles COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_emergency_vehicles.py" --check)
        add_test(NAME outland_generated_sedan COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_sedan_model.py" --check)
        add_test(NAME outland_generated_green_hand_car COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_green_hand_car.py" --check)
        add_test(NAME outland_generated_furniture COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_furniture.py" --check)
        add_test(NAME outland_generated_gang_outfits COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_gang_outfits.py" --check)
        add_test(NAME outland_generated_grenade_model COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_grenade_model.py" --check)
        add_test(NAME outland_generated_sight_models COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_sight_models.py" --check)
        add_test(NAME outland_generated_armour_models COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_armour_models.py" --check)
        add_test(NAME outland_generated_drop_models COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_drop_models.py" --check)
        add_test(NAME outland_generated_ranch_models COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_ranch_models.py" --check)
        add_test(NAME outland_generated_weapon_audio COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_weapon_audio.py" --check)
        add_test(NAME outland_city_kit_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_city_kit.py" --check)
        file(GLOB OUTLAND_TRAILER_MODELS "${PROJECT_SOURCE_DIR}/assets/verda/residential/trailers/*.glb")
        add_test(NAME outland_trailer_texture_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/convert_glb_jpeg_to_png.py" --check ${OUTLAND_TRAILER_MODELS})
        add_test(NAME outland_kit_slab_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_kit_slabs.py" --check)
        add_test(NAME outland_starter_catalog_validation COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/generate_starter_catalog.py" --check)
    endif()
    add_executable(outland_skeletal_gait_tests tests/integration/assets/test_skeletal_gait.cpp src/characters/SkeletalGait.cpp src/characters/AnimationController.cpp)
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
        add_executable(outland_navigation_tests tests/integration/navigation/test_navigation.cpp src/dev/VerdaWorldKit.cpp)
        target_link_libraries(outland_navigation_tests PRIVATE outland_render)
        target_compile_options(outland_navigation_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_navigation_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        add_test(NAME outland_navigation_tests COMMAND outland_navigation_tests)
        add_executable(outland_building_purpose_tests tests/integration/creator/test_building_purposes.cpp)
        target_link_libraries(outland_building_purpose_tests PRIVATE outland_render)
        target_compile_options(outland_building_purpose_tests PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
        target_compile_definitions(outland_building_purpose_tests PRIVATE OUTLAND_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        add_test(NAME outland_building_purpose_tests COMMAND outland_building_purpose_tests)

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
