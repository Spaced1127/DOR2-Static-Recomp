cmake_minimum_required(VERSION 3.24)

project(DOR2Recomp VERSION 0.1.1 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Branding and retail-disc identity.
#
# Set these before adding the ModernGekko runtime subdirectory. The disc ID and
# DOL hash make the shipped runner refuse a different dump instead of booting
# mismatched code.
# ---------------------------------------------------------------------------
set(MODERNGEKKO_FRONTEND_NAME
    "WWE Day of Reckoning 2 Recomp" CACHE STRING "" FORCE)
set(MODERNGEKKO_LAUNCHER_OUTPUT_NAME
    "DOR2Recomp" CACHE STRING "" FORCE)
set(MODERNGEKKO_RUNNER_OUTPUT_NAME
    "DOR2RecompRunnerCore" CACHE STRING "" FORCE)
set(MODERNGEKKO_USER_DIRECTORY_NAME
    "DOR2Recomp" CACHE STRING "" FORCE)
set(MODERNGEKKO_DEFAULT_WINDOW_TITLE
    "WWE Day of Reckoning 2" CACHE STRING "" FORCE)
set(MODERNGEKKO_LOG_FILENAME
    "ModernGekko.log" CACHE STRING "" FORCE)

set(MODERNGEKKO_REQUIRED_DISC_ID
    "GW2E78" CACHE STRING "" FORCE)
set(MODERNGEKKO_REQUIRED_DOL_SHA256
    "e47abf73e2c7162098c01de34b90ab66031c3f161fd72b36dde0173ea4af3610"
    CACHE STRING "" FORCE)

option(MODERNGEKKO_PORTABLE_DEFAULT_GAME
       "Keep default-game.txt beside the executables" ON)
option(MODERNGEKKO_GAMECUBE_CONTROLLERS
       "Generate GameCube controller profiles" ON)
set(MODERNGEKKO_ENABLE_DOLPHIN_RUNTIME ON CACHE BOOL "" FORCE)

# Icon search path for rc.exe.
if(WIN32)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I \"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")
endif()

add_subdirectory(lib/ModernGekko-Template/lib/ModernGekko
                 "${CMAKE_BINARY_DIR}/moderngekko-runtime")

# Keep every shipped executable side by side.
set(DOR2_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}")

foreach(target moderngekko-run moderngekko-launcher)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${DOR2_RUNTIME_OUTPUT_DIR}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Windows resources (icon + version info). Attached from this project layer so
# the upstream runtime sources stay untouched.
# ---------------------------------------------------------------------------
if(WIN32)
    if(TARGET moderngekko-launcher)
        target_sources(moderngekko-launcher PRIVATE assets/launcher.rc)
    endif()
    if(TARGET moderngekko-run)
        target_sources(moderngekko-run PRIVATE assets/runner-core.rc)
    endif()

    # UTF-8 ANSI code page for every tool that touches the extracted game tree.
    # Disc names are Shift-JIS, so wide-to-narrow path conversion throws under
    # the legacy code page (see assets/app.manifest).
    set(DOR2_MANIFESTED_TARGETS
        moderngekko-run
        moderngekko-launcher
        moderngekko-port
        dolrecomp)
    foreach(target ${DOR2_MANIFESTED_TARGETS})
        if(TARGET ${target})
            target_sources(${target} PRIVATE assets/app.manifest)
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Launcher artwork (SteamGridDB, user-approved override of the original-art
# policy; see docs/ASSETS.md). Copied next to the launcher so it can load them;
# the launcher falls back to procedural art when they are absent.
# ---------------------------------------------------------------------------
if(WIN32)
  # Static launcher artwork (SteamGridDB, user-approved; see docs/ASSETS.md).
  # Copied next to the launcher at configure time; the launcher falls back to
  # procedural art when the files are absent.
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/sgdb/dor2_grid.png"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sgdb/dor2_logo.png"
       DESTINATION "${CMAKE_BINARY_DIR}")
endif()

# ---------------------------------------------------------------------------
# DOR2RecompRunner: user-facing runner with --core static|jit. It starts
# DOR2RecompRunnerCore.exe after selecting the CPU core.
# ---------------------------------------------------------------------------
add_executable(DOR2RecompRunner src/runner-shim/main.cpp)
target_compile_features(DOR2RecompRunner PRIVATE cxx_std_17)
set_target_properties(DOR2RecompRunner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DOR2_RUNTIME_OUTPUT_DIR}"
    OUTPUT_NAME "DOR2RecompRunner")
if(MSVC)
    target_compile_options(DOR2RecompRunner PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(DOR2RecompRunner PRIVATE -Wall -Wextra)
endif()
if(WIN32)
    target_sources(DOR2RecompRunner PRIVATE assets/runner.rc assets/app.manifest)
endif()

# ---------------------------------------------------------------------------
# DOR2Prepare: headless ISO extractor for RecompLauncher's prepare step.
# ---------------------------------------------------------------------------
add_executable(DOR2Prepare src/prepare/main.cpp)
target_compile_features(DOR2Prepare PRIVATE cxx_std_17)
set_target_properties(DOR2Prepare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DOR2_RUNTIME_OUTPUT_DIR}"
    OUTPUT_NAME "DOR2Prepare")

if(MSVC)
    target_compile_options(DOR2Prepare PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(DOR2Prepare PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_sources(DOR2Prepare PRIVATE assets/prepare.rc assets/app.manifest)
endif()
