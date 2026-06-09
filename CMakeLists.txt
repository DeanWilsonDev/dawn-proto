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
# cimmerian is the test framework for dawn_tests. Guarded for the same reason as
# amanuensis: amanuensis/firefly bundle it but only add it when they are the root
# project, so here we add it ourselves exactly once.
if(NOT TARGET cimmerian)
    add_subdirectory(libs/cimmerian)
endif()

# -----------------------------------------------------------------------
# Dawn — the editor executable
#
# The source list grows one milestone at a time toward the brief's final
# CMakeLists. M0 builds only the window shell, theme, and resolvers.
# -----------------------------------------------------------------------
# Pure-logic modules: no SDL, Penumbra, or Firefly. Shared between dawn and the
# SDL-free dawn_tests binary.
set(DAWN_CORE_SOURCES
    src/SceneDocument.cpp
    src/SceneCommand.cpp
    src/SceneSerialiser.cpp
    src/UuidGenerator.cpp
)

add_executable(dawn
    src/main.cpp
    src/DawnTheme.cpp
    src/DawnResolvers.cpp
    src/EditorApplication.cpp
    ${DAWN_CORE_SOURCES}
)
target_link_libraries(dawn PRIVATE penumbra amanuensis firefly)
target_compile_definitions(dawn PRIVATE
    DAWN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# -----------------------------------------------------------------------
# dawn_tests — pure-logic tests, NO penumbra or SDL dependency.
# Compiles the core modules directly and links only amanuensis + cimmerian.
# -----------------------------------------------------------------------
add_executable(dawn_tests
    tests/test-main.cpp
    tests/serialisation.test.cpp
    tests/commands.test.cpp
    ${DAWN_CORE_SOURCES}
)
target_include_directories(dawn_tests PRIVATE src)
target_link_libraries(dawn_tests PRIVATE amanuensis cimmerian)
