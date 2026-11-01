cmake_minimum_required(VERSION 3.20)
project(doublesteal2_recomp C)

# Double-S.T.E.A.L: The Second Clash (Xbox, 2004) - Static Recompilation
#
# Built the way sp00nznet/wreckless builds the first game: the toolkit is a
# dependency beside this checkout (antbox, as ../xboxrecomp-takedawn if that
# is there, else ../antbox), not a copy. This repository holds only the
# per-title work.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp-takedawn")
    set(DS2_DEFAULT_TOOLKIT "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp-takedawn")
else()
    set(DS2_DEFAULT_TOOLKIT "${CMAKE_CURRENT_SOURCE_DIR}/../antbox")
endif()
set(XBOXRECOMP_DIR "${DS2_DEFAULT_TOOLKIT}" CACHE PATH "Path to the toolkit (antbox)")

# Off Windows the kernel links OpenSSL::Crypto but finds it in its own
# subdirectory, where the imported target is invisible to the executable;
# finding it here first puts it in scope for both.
if(NOT WIN32)
    find_package(OpenSSL REQUIRED)
endif()

# The runtime's opt-in enhancements layer (enhance.toml: render scale, the
# dashboard's HDTV and widescreen settings, the window's filter and aspect).
# On by default for this title; the toolkit's own default is off.
if(NOT DEFINED CACHE{XBOXRECOMP_ENHANCE})
    set(XBOXRECOMP_ENHANCE ON CACHE BOOL "Build the opt-in enhancements layer (src/enhance)")
endif()
# The runtime's in-game settings overlay (OPTIONS > PC SETTING, Back + Start,
# F1; src/game/settings.c). On for this title too; it needs C++ and SDL2.
if(NOT DEFINED CACHE{XBOXRECOMP_OVERLAY})
    set(XBOXRECOMP_OVERLAY ON CACHE BOOL "Build the in-game settings overlay (Dear ImGui)")
endif()

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# This title's defaults for runtime keys (src/game/env/recomp_env_game.h).
# PUBLIC, so every part of the runtime reads the same table.
if(TARGET recomp_env AND EXISTS "${XBOXRECOMP_DIR}/src/platform/recomp_env.h")
    target_compile_definitions(recomp_env PUBLIC RECOMP_ENV_HAVE_GAME_KEYS)
    target_include_directories(recomp_env PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/game/env)
endif()
add_subdirectory(src/game)
