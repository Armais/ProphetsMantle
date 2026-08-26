cmake_minimum_required(VERSION 3.24)
project(MANTLE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# =============================================================================
# Platform detection
# =============================================================================
if(EMSCRIPTEN)
    set(PLATFORM_WEB TRUE)
    message(STATUS "Building for Web (Emscripten)")
elseif(ANDROID)
    set(PLATFORM_ANDROID TRUE)
    message(STATUS "Building for Android")
elseif(IOS)
    set(PLATFORM_IOS TRUE)
    message(STATUS "Building for iOS")
else()
    set(PLATFORM_DESKTOP TRUE)
    message(STATUS "Building for Desktop (${CMAKE_SYSTEM_NAME})")
endif()

# =============================================================================
# SDL3 Dependencies (auto-downloaded via FetchContent)
# =============================================================================
if(NOT PLATFORM_WEB)
    message(STATUS "Using local SDL3 dependencies...")

    # SDL3 core (local)
    FetchContent_Declare(
        SDL3
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/build/_deps/sdl3-src
        OVERRIDE_FIND_PACKAGE
    )

    # SDL3_image (local)
    FetchContent_Declare(
        SDL3_image
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/build/_deps/sdl3_image-src
        OVERRIDE_FIND_PACKAGE
    )

    # SDL3_mixer (local)
    FetchContent_Declare(
        SDL3_mixer
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/build/_deps/sdl3_mixer-src
        OVERRIDE_FIND_PACKAGE
    )

    # SDL3_ttf (local: release 3.2.2 + external/freetype clone, branch VER-2-13-2-SDL)
    FetchContent_Declare(
        SDL3_ttf
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/build/_deps/sdl3_ttf-src
        OVERRIDE_FIND_PACKAGE
    )

    # Configure SDL options before fetching
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)

    # SDL3_image: only need PNG/BMP for pixel art game
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_BMP ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_QOI ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)

    # SDL3_mixer
    set(SDLMIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)

    # SDL3_ttf: vendored FreeType only (no HarfBuzz shaping, no SVG emoji)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(SDL3 SDL3_image SDL3_mixer SDL3_ttf)
endif()

# =============================================================================
# Source files
# =============================================================================
set(MANTLE_SOURCES
    src/main.cpp
    src/core/engine.cpp
    src/core/ecs.cpp
    src/core/i18n.cpp
    src/render/renderer.cpp
    src/render/sprites.cpp
    src/render/camera.cpp
    src/input/input.cpp
    src/audio/audio.cpp
    src/game/player.cpp
    src/game/faith.cpp
    src/game/miracles.cpp
    src/game/enemies.cpp
    src/game/items.cpp
    src/game/rooms.cpp
    src/game/combat.cpp
    src/ui/hud.cpp
    src/ui/menus.cpp
    src/ui/story.cpp
    src/ui/cinematic.cpp
    src/ui/codex.cpp
    src/systems/save.cpp
    src/systems/progression.cpp
    src/systems/achievements.cpp
)

# =============================================================================
# Executable
# =============================================================================
# Icono del .exe: MSVC compila el .rc sin necesidad de nada mas. Solo en Windows -- en
# Linux/Mac el icono lo pone SDL_SetWindowIcon (Engine::init) y el .desktop / .app.
if(WIN32)
    list(APPEND MANTLE_SOURCES mantle.rc)
endif()

if(PLATFORM_ANDROID)
    add_library(${PROJECT_NAME} SHARED ${MANTLE_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${MANTLE_SOURCES})
endif()

# Output executable to project root (all configs)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# =============================================================================
# Link SDL3
# =============================================================================
if(PLATFORM_WEB)
    target_compile_options(${PROJECT_NAME} PRIVATE -sUSE_SDL=3 -sUSE_SDL_IMAGE=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=3)
    target_link_options(${PROJECT_NAME} PRIVATE
        -sUSE_SDL=3
        -sUSE_SDL_IMAGE=2
        -sUSE_SDL_MIXER=2
        -sUSE_SDL_TTF=3
        -sALLOW_MEMORY_GROWTH=1
        -sMAX_WEBGL_VERSION=2
        -sMIN_WEBGL_VERSION=2
        -sEXPORTED_FUNCTIONS=_main
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
        -sINITIAL_MEMORY=67108864
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3::SDL3-static
        SDL3_image::SDL3_image-static
        SDL3_mixer::SDL3_mixer-static
        SDL3_ttf::SDL3_ttf-static
    )
endif()

# =============================================================================
# Platform-specific configuration
# =============================================================================
if(PLATFORM_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_PLATFORM_DESKTOP)

    # Steamworks SDK (optional)
    if(EXISTS "${CMAKE_SOURCE_DIR}/vendor/steamworks/sdk")
        target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_STEAM)
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/vendor/steamworks/sdk/public
        )
        if(WIN32)
            target_link_libraries(${PROJECT_NAME} PRIVATE
                ${CMAKE_SOURCE_DIR}/vendor/steamworks/sdk/redistributable_bin/win64/steam_api64.lib
            )
        endif()
    endif()

elseif(PLATFORM_WEB)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_PLATFORM_WEB)

elseif(PLATFORM_ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_PLATFORM_ANDROID)

elseif(PLATFORM_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_PLATFORM_IOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.mantle.prophet"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

# Mobile shared definitions
if(PLATFORM_ANDROID OR PLATFORM_IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MANTLE_PLATFORM_MOBILE)
endif()

# =============================================================================
# Compiler warnings
# =============================================================================
if(MSVC)
    # /utf-8: the sources hold Spanish text (Reina-Valera quotes, accented names) as UTF-8
    # literals. Without this MSVC reads them as codepage 1252 and every tilde turns to mojibake.
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4100 /wd4189 /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# =============================================================================
# Assets - copy to build directory
# =============================================================================
if(PLATFORM_DESKTOP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()
