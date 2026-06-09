cmake_minimum_required(VERSION 3.24)
project(Dawn VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL3 is pulled in transitively via penumbra; Dawn needs the headers here because
# Penumbra's public widget API exposes SDL types (SDL_Color, SDL_FRect, InputState).
find_package(SDL3     CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

# Dependency wiring note: firefly bundles its own amanuensis submodule and adds it
# unconditionally, so it creates the `amanuensis` target. Adding libs/amanuensis as
# well would collide on that target name. Both copies are pinned to the same commit,
# so we let firefly provide `amanuensis` and guard our own add as a fallback. This
# keeps every shared target defined exactly once with no submodule edits.
add_subdirectory(libs/penumbra)
add_subdirectory(libs/firefly)
if(NOT TARGET amanuensis)
    add_subdirectory(libs/amanuensis)
endif()
# Note: libs/cimmerian is added in M1 alongside the dawn_tests target.

# -----------------------------------------------------------------------
# Dawn — the editor executable
#
# The source list grows one milestone at a time toward the brief's final
# CMakeLists. M0 builds only the window shell, theme, and resolvers.
# -----------------------------------------------------------------------
add_executable(dawn
    src/main.cpp
    src/DawnTheme.cpp
    src/DawnResolvers.cpp
    src/EditorApplication.cpp
)
target_link_libraries(dawn PRIVATE penumbra amanuensis firefly)
target_compile_definitions(dawn PRIVATE
    DAWN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
