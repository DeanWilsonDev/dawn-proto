# Dawn — Prototype Session Brief

> **This document is the authoritative scope for this session.**
> The Dawn docs in `fearless-hq` describe the full vision.
> This document describes exactly what to build right now and what to skip.

---

## What This Session Is

A focused prototype to answer one question: **can Dawn's UI, scene management, and
serialisation work together on top of Penumbra?**

The Penumbra PoC is proven and running. Dawn does not yet exist. This session produces a
minimal but real Dawn executable that:

- Looks like Dawn (the amber dark theme, not the Penumbra demo aesthetic)
- Hosts a `ViewportWidget` where entities are visible and interactive
- Reads and writes scene JSON via Amanuensis
- Supports placement, selection, movement, and undo/redo
- Proves the Penumbra → Dawn consumption model is workable

This is throwaway-grade, like the Penumbra PoC before it. The goal is to learn whether
the shape is right, not to ship.

---

## Repositories

All four are first-party libraries. Add them as git submodules under `libs/`:

```bash
git submodule add https://github.com/DeanWilsonDev/penumbra-proto  libs/penumbra
git submodule add https://github.com/DeanWilsonDev/amanuensis      libs/amanuensis
git submodule add https://github.com/DeanWilsonDev/firefly         libs/firefly
git submodule add https://github.com/DeanWilsonDev/cimmerian       libs/cimmerian
git submodule update --init --recursive
```

| Library | Role | GitHub |
| --- | --- | --- |
| penumbra-proto | UI framework (SDL3, retained-mode) | https://github.com/DeanWilsonDev/penumbra-proto |
| amanuensis | JSON read/write | https://github.com/DeanWilsonDev/amanuensis |
| firefly | Logging | https://github.com/DeanWilsonDev/firefly |
| cimmerian | Testing (BDD-style) | https://github.com/DeanWilsonDev/cimmerian |

---

## Documentation Reference

All project documentation lives at **https://github.com/DeanWilsonDev/fearless-hq**.
This is the canonical source for everything about the Umbra ecosystem — engine design,
Dawn's full specification, Crimson & Thorn's game design, and all architectural decisions
made to date. If you need context on *why* something is designed the way it is, or want
to see the full long-term vision beyond this PoC's scope, that repo is where to look.

---

## Required Reading Before Writing Any Code

1. **`libs/penumbra/docs/penumbra_poc_spec.md`** — the Penumbra architecture, layer
   responsibilities, widget catalogue, and token/theme model. Do not assume anything
   about Penumbra; read it.
2. **`libs/penumbra/demo/main.cpp`** — the working example of how a consuming app builds
   its theme, resolvers, and widget tree. Dawn follows this pattern exactly.
3. **https://github.com/DeanWilsonDev/fearless-hq** → `Projects/umbra-game-engine/dawn-2d-scene-editor/design-document.md` —
   Dawn's intended architecture: SceneDocument, command pattern, ViewportWidget seam,
   scene JSON format. This is the design Dawn is being built toward.

---

## Library APIs

### Amanuensis — JSON

```cpp
#include <amanuensis.hpp>   // umbrella header — at the include root, not a subdirectory
```

CMake: `add_subdirectory(libs/amanuensis)` → `target_link_libraries(... PRIVATE amanuensis)`

**Reading:**
```cpp
// From file — never throws on parse failure; check succeeded
auto Result = Amanuensis::Reader::ParseFile("scene.json");
if (!Result.succeeded) {
    LOG_ERROR("Parse failed at {}:{} — {}", Result.error.line,
              Result.error.column, Result.error.message);
    return;
}
Amanuensis::Value Root = Result.value;

// From string
auto Result2 = Amanuensis::Reader::ParseString(R"({"x": 1})");
```

**Writing:**
```cpp
Amanuensis::Value Root = Amanuensis::Value::MakeObject();
Root.Insert("version", 1);
Root.Insert("name", std::string("scene_01"));

std::string Text = Amanuensis::Writer::WriteToString(Root);   // pretty-printed
bool Ok = Amanuensis::Writer::WriteToFile(Root, "output.json"); // returns false on I/O failure
```

**The Value type:**
```cpp
// Construction
Amanuensis::Value Null;
Amanuensis::Value Bool  = true;
Amanuensis::Value Int   = 42LL;        // note: long long for integers
Amanuensis::Value Float = 3.14;
Amanuensis::Value Text  = std::string("hello");  // use std::string, not const char*
Amanuensis::Value Arr   = Amanuensis::Value::MakeArray();
Amanuensis::Value Obj   = Amanuensis::Value::MakeObject();

// Type inspection
value.IsNull(); value.IsBoolean(); value.IsInteger(); value.IsDouble();
value.IsNumber();   // true for Integer or Double
value.IsString(); value.IsArray(); value.IsObject();

// Typed accessors — throw TypeMismatchError on wrong type; check type first
bool         b = value.AsBoolean();
long long    i = value.AsInteger();
double       d = value.AsDouble();
std::string  s = value.AsString();

// Array
Arr.PushBack(99LL);
std::size_t Count = Arr.Size();
Amanuensis::Value& Element = Arr.At(0);

// Object — insertion order preserved on write
Obj.Insert("key", std::string("value"));
bool Exists = Obj.Contains("key");
Amanuensis::Value& V = Obj.Get("key");          // throws if absent
const Amanuensis::Value* P = Obj.Find("key");   // nullptr if absent
```

**Auto-serialisation via `AMANUENSIS_SERIALISABLE` (use for simple structs):**
```cpp
struct Transform {
    double PositionX{0.0};
    double PositionY{0.0};
    double Rotation{0.0};
    double ScaleX{1.0};
    double ScaleY{1.0};
};
AMANUENSIS_SERIALISABLE(Transform, PositionX, PositionY, Rotation, ScaleX, ScaleY);

// Then:
Transform T{100.0, 200.0, 0.0, 1.0, 1.0};
Amanuensis::Value V = Amanuensis::ToJson(T);                     // serialize
Transform T2 = Amanuensis::FromJson<Transform>(V);               // deserialize
auto TryResult = Amanuensis::TryFromJson<Transform>(V);          // non-throwing
if (!TryResult.succeeded) { LOG_ERROR("{}", TryResult.errorMessage); }
```

> **Warning:** `AMANUENSIS_SERIALISABLE` does not work with template types in macro
> arguments directly — bare commas confuse the preprocessor. For `std::vector<T>`,
> use a `using` alias: `using EntityList = std::vector<EntityData>;` and declare the
> field as `EntityList Entities;`. Or handle `std::vector` fields manually.
> `std::vector<T>`, `std::optional<T>`, and `std::map<std::string, T>` are supported
> as field types as long as the element type itself has opted in.

**Manual serialisation for nested/complex types (SceneDocument approach):**
```cpp
// Building a scene JSON object manually:
Amanuensis::Value SceneJson = Amanuensis::Value::MakeObject();
Amanuensis::Value SceneMeta = Amanuensis::Value::MakeObject();
SceneMeta.Insert("name", Doc.SceneName);
SceneJson.Insert("scene", SceneMeta);

Amanuensis::Value EntityArray = Amanuensis::Value::MakeArray();
for (const auto& Entity : Doc.Entities) {
    Amanuensis::Value E = Amanuensis::Value::MakeObject();
    E.Insert("id",   Entity.Id);
    E.Insert("name", Entity.Name);
    E.Insert("type", Entity.Type);
    // ...
    EntityArray.PushBack(E);
}
SceneJson.Insert("entities", EntityArray);
```

---

### Firefly — Logging

```cpp
#include <firefly/log.hpp>
#include <firefly/log-registry.hpp>
#define FIREFLY_DEFAULT_LOGGER "Dawn"   // redefine AFTER the include
                                        // log.hpp does an unguarded define of "Client",
                                        // so a pre-include define gets overwritten.
                                        // The macro is expanded at each LOG_* call site,
                                        // so the later redefinition takes effect correctly.
```

CMake: `add_subdirectory(libs/firefly)` → `target_link_libraries(... PRIVATE firefly)`

**Setup (call once in `main` before anything else logs):**
```cpp
// Console only
Firefly::LogRegistry::RegisterLogger("Dawn");

// Console + CSV file, with debug logging enabled
Firefly::LogRegistry::RegisterLogger("Dawn", "log.csv", true);
```

**Logging macros (target `FIREFLY_DEFAULT_LOGGER`, which we set to "Dawn"):**
```cpp
LOG_TRACE("Viewport resized to {}x{}", Width, Height);
LOG_DEBUG("Entity {} placed at ({}, {})", Entity.Id, X, Y);
LOG_INFO("Scene loaded: {}", ScenePath);
LOG_WARNING("Schema file not found, using defaults");
LOG_ERROR("Failed to parse scene JSON at {}:{}", Line, Col);
LOG_FATAL("Unrecoverable state in command stack");
```

> `LOG_TRACE` and `LOG_DEBUG` are suppressed unless the logger was registered with
> `enableDebugLogging = true`.

**Direct logger access (when you need it outside the macros):**
```cpp
auto& Logger = Firefly::LogRegistry::GetLogger("Dawn");
Logger->Info("Scene saved in {} ms", ElapsedMs);
```

---

### Cimmerian — Testing

```cpp
#include <cimmerian/test.hpp>
```

CMake for a **separate test binary** (never link cimmerian into the main `dawn` executable):
```cmake
add_executable(dawn_tests
    tests/test-main.cpp
    tests/serialisation.test.cpp
    tests/commands.test.cpp
)
target_link_libraries(dawn_tests PRIVATE amanuensis cimmerian)
# Note: dawn_tests does NOT link against penumbra or SDL3.
# It only tests pure-logic modules: SceneDocument, SceneSerialiser, commands.
```

**Test entry point (`tests/test-main.cpp`):**
```cpp
#include <cimmerian/test-entry-point.hpp>
// That's it. The macro provides main().
```

**Writing tests:**
```cpp
#include <cimmerian/test.hpp>
#include "SceneSerialiser.h"

DESCRIBE("SceneSerialiser", {
    IT("round-trips a scene without data loss", {
        const std::string OriginalJson = R"({
            "scene": { "name": "test", "dimensions": {"width": 3840, "height": 1080} },
            "layers": [],
            "entities": []
        })";

        auto ParseResult = Amanuensis::Reader::ParseString(OriginalJson);
        REQUIRE_TRUE(ParseResult.succeeded);

        SceneDocument Doc;
        SceneSerialiser::Deserialise(ParseResult.value, Doc);

        Amanuensis::Value Written = SceneSerialiser::Serialise(Doc);
        std::string RoundTripped = Amanuensis::Writer::WriteToString(Written);

        // Verify key fields survived the round-trip
        auto ReParseResult = Amanuensis::Reader::ParseString(RoundTripped);
        REQUIRE_TRUE(ReParseResult.succeeded);
        ASSERT_EQUAL(
            ReParseResult.value.Get("scene").Get("name").AsString(),
            std::string("test")
        );
    });

    IT("deserialises entity transform correctly", {
        // ...
    });
});

DESCRIBE("PlaceEntityCommand", {
    IT("adds an entity to the document", {
        SceneDocument Doc;
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});

        PlaceEntityCommand Cmd("platform", 100.0f, 200.0f);
        Cmd.Execute(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_EQUAL(Doc.Entities[0].Type, std::string("platform"));
    });

    IT("undoes cleanly", {
        SceneDocument Doc;
        PlaceEntityCommand Cmd("platform", 100.0f, 200.0f);
        Cmd.Execute(Doc);
        Cmd.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});
    });
});
```

**Assertion reference:**

| Macro | Behaviour |
| --- | --- |
| `ASSERT_TRUE(cond)` | Fails and continues |
| `ASSERT_FALSE(cond)` | Fails and continues |
| `ASSERT_EQUAL(a, b)` | Fails and continues; shows diff |
| `ASSERT_NOT_EQUAL(a, b)` | Fails and continues |
| `REQUIRE_TRUE(cond)` | Fails and **stops** the test |
| `REQUIRE_EQUAL(a, b)` | Fails and **stops** the test |

---

## What Penumbra Provides Right Now

Available and proven:

- `Penumbra::Platform::PlatformWindow` — SDL3 window, event loop, input snapshot, DPI,
  clipboard
- `Penumbra::Render::Renderer` — `DrawFilledRect`, `DrawRectOutline`, `DrawText`,
  `DrawTexture`, clip stack, DPI-correct rendering
- `Penumbra::Render::SdlTtfFontBackend` — font loading and text measurement
- `Penumbra::Widgets::Box`, `Button`, `Label`, `Checkbox`, `NumericDrag`, `TextInput`,
  `ScrollablePanel` — the full primitive widget set
- `Penumbra::Widgets::ViewportWidget` — render-to-texture seam; `OnRenderScene` callback
  for scene drawing; `OnSceneInput` callback for scene input. Dawn never calls SDL directly.
- `Penumbra::Anim::AnimatedColor`, `Tween` — framerate-independent easing

**Not available in Penumbra (do not add to Penumbra this session):**

- No `SelectWidget` / dropdown — work around with a vertical list of Buttons
- No `SplitterWidget` — panel widths are fixed fractions of the window
- No `TabBarWidget` — use a plain Label as a panel section header
- No image/sprite loading — Penumbra does not load image files. Scene entities are drawn
  as coloured rectangles in this PoC. Sprite rendering is deferred.
- No built-in file dialogs — the `.dawn` project file path is passed as a command-line
  argument: `./dawn path/to/project.dawn`

---

## What to Build (PoC Scope)

Only build what is listed here. Do not add features speculatively.

- Dawn executable linking Penumbra, Amanuensis, and Firefly
- `DawnTheme` struct and `DawnResolvers` (see theme values below)
- Main window layout: fixed left strip (entity palette), central `ViewportWidget`, fixed
  right strip (properties panel)
- `SceneDocument`: entity list, layer list, dirty flag, no knowledge of UI or files
- `SceneCommand` base class, command stack, `CompoundCommand`
- UUID v4 generation (stdlib `<random>`, no additional dependency)
- `SceneSerialiser` using Amanuensis: read `.dawn` project file, read entity schema JSON,
  read/write scene JSON
- `ViewportWidget` wired up: entities drawn as coloured rectangles; camera pan (middle
  mouse drag) and zoom (scroll wheel, centred on cursor) via `OnSceneInput`
- Entity palette panel: vertical list of Buttons, one per entity type loaded from schema
- Click in viewport to place entity → `PlaceEntityCommand`
- Click entity to select (highlight in viewport); drag to move → `MoveEntityCommand`
- Properties panel: entity name (`TextInput`), position x and y (`NumericDrag`)
- Undo (Ctrl+Z) and redo (Ctrl+Shift+Z)
- Save scene (Ctrl+S) via Amanuensis
- Reload scene from disk (Ctrl+R) with dirty confirmation prompt
- Dirty indicator: a `Label` showing `*` when there are unsaved changes
- Entity list in left panel showing entity names as selectable Labels
- Firefly logging throughout: INFO for lifecycle events, WARNING for missing data,
  ERROR for parse/IO failures
- `dawn_tests` binary (Cimmerian) covering: serialisation round-trip, place/undo command

## What NOT to Build

- Sprite or image rendering (no stb_image, no AssetCache, no SDL texture loading for assets)
- Layer management panel (layers exist in SceneDocument and JSON but are not UI-editable)
- Native file dialogs (nfd)
- Transform gizmo handles
- Draggable panel splitters
- Schema hot-reload
- Tab bars
- Preferences persistence
- Multiple entity selection

---

## CMakeLists Structure

```cmake
cmake_minimum_required(VERSION 3.24)
project(Dawn VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3     CONFIG REQUIRED)   # transitive via penumbra — needed here for SDL types
find_package(SDL3_ttf CONFIG REQUIRED)

add_subdirectory(libs/penumbra)
add_subdirectory(libs/amanuensis)
add_subdirectory(libs/firefly)
add_subdirectory(libs/cimmerian)

# -----------------------------------------------------------------------
# Dawn — the editor executable
# -----------------------------------------------------------------------
add_executable(dawn
    src/main.cpp
    src/DawnTheme.cpp
    src/DawnResolvers.cpp
    src/SceneDocument.cpp
    src/SceneCommand.cpp
    src/SceneSerialiser.cpp
    src/EditorApplication.cpp
    src/UuidGenerator.cpp
)
target_link_libraries(dawn PRIVATE penumbra amanuensis firefly)
target_compile_definitions(dawn PRIVATE
    DAWN_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# -----------------------------------------------------------------------
# dawn_tests — pure-logic tests, NO penumbra or SDL dependency
# -----------------------------------------------------------------------
add_executable(dawn_tests
    tests/test-main.cpp
    tests/serialisation.test.cpp
    tests/commands.test.cpp
)
target_include_directories(dawn_tests PRIVATE src)
target_link_libraries(dawn_tests PRIVATE amanuensis cimmerian)
```

Usage: `./build/dawn data/test.dawn`

---

## Camera Transform

The `ViewportWidget` provides the rendering seam but no camera. Dawn owns it.

```cpp
// In EditorApplication — persists between frames:
struct CameraState {
    float OffsetX{0.0f};
    float OffsetY{0.0f};
    float Zoom{1.0f};
};

// Inside OnRenderScene — world-to-screen transform:
SceneViewport->OnRenderScene = [&](Penumbra::Render::Renderer& R, SDL_FPoint SceneSize) {
    for (const auto& Entity : SceneDoc.Entities) {
        const float ScreenX = (Entity.PositionX - Camera.OffsetX) * Camera.Zoom;
        const float ScreenY = (Entity.PositionY - Camera.OffsetY) * Camera.Zoom;
        const float ScreenW = Entity.Width  * Camera.Zoom;
        const float ScreenH = Entity.Height * Camera.Zoom;
        R.DrawFilledRect({ScreenX, ScreenY, ScreenW, ScreenH}, Entity.EditorColor);
        if (Entity.Id == SelectedEntityId) {
            R.DrawRectOutline({ScreenX - 2, ScreenY - 2, ScreenW + 4, ScreenH + 4},
                              DawnTheme.ColorAccent, 2.0f);
        }
    }
};

// Inside OnSceneInput — pan and zoom:
SceneViewport->OnSceneInput = [&](const Penumbra::Platform::InputState& Input,
                                   SDL_FRect ContentRect) -> bool {
    // Pan: middle mouse drag
    if (Input.MouseButtonDown[1]) {   // middle button index 1
        Camera.OffsetX -= (Input.MousePosition.x - LastMousePos.x) / Camera.Zoom;
        Camera.OffsetY -= (Input.MousePosition.y - LastMousePos.y) / Camera.Zoom;
    }
    // Zoom: scroll wheel, centred on cursor
    if (Input.MouseWheelDelta != 0.0f) {
        const float WorldX = (Input.MousePosition.x - ContentRect.x) / Camera.Zoom + Camera.OffsetX;
        const float WorldY = (Input.MousePosition.y - ContentRect.y) / Camera.Zoom + Camera.OffsetY;
        Camera.Zoom = std::clamp(Camera.Zoom * (1.0f + Input.MouseWheelDelta * 0.1f), 0.1f, 10.0f);
        Camera.OffsetX = WorldX - (Input.MousePosition.x - ContentRect.x) / Camera.Zoom;
        Camera.OffsetY = WorldY - (Input.MousePosition.y - ContentRect.y) / Camera.Zoom;
    }
    LastMousePos = Input.MousePosition;
    return true;
};
```

---

## Dawn Theme

Dawn does not use the Penumbra demo palette. Define a `DawnTheme` struct in Dawn, same
pattern as `Demo::Theme` in the Penumbra demo. Penumbra sees none of these names.

```cpp
namespace Dawn {

struct Theme {
    SDL_Color ColorBackgroundBase    = { 15,  15,  18, 255}; // #0F0F12
    SDL_Color ColorSurface           = { 26,  26,  31, 255}; // #1A1A1F
    SDL_Color ColorSurfaceRaised     = { 36,  36,  41, 255}; // #242429
    SDL_Color ColorSurfaceOverlay    = { 46,  46,  53, 255}; // #2E2E35

    SDL_Color ColorBorderSubtle      = { 58,  58,  68, 255}; // #3A3A44
    SDL_Color ColorBorderFocus       = { 90,  90, 104, 255}; // #5A5A68

    SDL_Color ColorTextPrimary       = {234, 234, 240, 255}; // #EAEAF0
    SDL_Color ColorTextSecondary     = {144, 144, 160, 255}; // #9090A0
    SDL_Color ColorTextDisabled      = { 80,  80,  96, 255}; // #505060

    SDL_Color ColorAccent            = {232, 148,  58, 255}; // #E8943A  amber
    SDL_Color ColorAccentHovered     = {242, 168,  78, 255};
    SDL_Color ColorAccentPressed     = {212, 128,  38, 255};

    SDL_Color ColorWarning           = {232, 200,  74, 255}; // dirty indicator
    SDL_Color ColorDestructive       = {217,  96,  96, 255};

    SDL_Color ColorEntityPlatform    = { 74, 144, 217, 255}; // #4A90D9
    SDL_Color ColorEntityProp        = {126, 211,  33, 255}; // #7ED321
    SDL_Color ColorEntityTrigger     = {245, 166,  35, 255}; // #F5A623
    SDL_Color ColorEntityCharacter   = {189,  16, 224, 255};

    float SpacingSmall   =  6.0f;
    float SpacingMedium  = 10.0f;
    float SpacingLarge   = 16.0f;

    float PanelStripWidth    = 44.0f;
    float PanelContentWidth  = 220.0f;
    float StatusBarHeight    = 24.0f;

    float FontSizeBody   = 13.0f;
    float FontSizeSmall  = 11.0f;

    float BorderRadiusPanel  = 8.0f;
    float BorderRadiusButton = 5.0f;
    float BorderWidthDefault = 1.0f;
    float AnimColorSeconds   = 0.08f;
};

} // namespace Dawn
```

---

## Bootstrap Data Files

These ship in `data/` so the prototype has something to open immediately.

**`data/test.dawn`**
```json
{
  "project": { "name": "Dawn PoC" },
  "paths": {
    "schemaPath": "data/entity_schema.json",
    "assetsRoot": "assets/",
    "scenesRoot": "data/scenes/"
  }
}
```

**`data/entity_schema.json`**
```json
{
  "entityTypes": [
    { "id": "platform",  "displayName": "Platform",  "editorColor": "#4A90D9",
      "defaultWidth": 200.0, "defaultHeight": 32.0 },
    { "id": "character", "displayName": "Character", "editorColor": "#BD10E0",
      "defaultWidth": 48.0,  "defaultHeight": 96.0 },
    { "id": "trigger",   "displayName": "Trigger",   "editorColor": "#F5A623",
      "defaultWidth": 64.0,  "defaultHeight": 64.0 }
  ]
}
```

**`data/scenes/test_scene.json`**
```json
{
  "scene": { "name": "test_scene", "dimensions": { "width": 3840, "height": 1080 } },
  "layers": [
    { "id": "background", "order": 0, "visible": true, "locked": false },
    { "id": "midground",  "order": 1, "visible": true, "locked": false }
  ],
  "entities": [
    {
      "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "name": "floor_left",
      "type": "platform",
      "layer": "midground",
      "transform": { "positionX": 100.0, "positionY": 800.0,
                     "rotation": 0.0, "scaleX": 1.0, "scaleY": 1.0 },
      "properties": { "width": 400.0, "height": 32.0 }
    },
    {
      "id": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      "name": "player_spawn",
      "type": "character",
      "layer": "midground",
      "transform": { "positionX": 200.0, "positionY": 700.0,
                     "rotation": 0.0, "scaleX": 1.0, "scaleY": 1.0 },
      "properties": { "width": 48.0, "height": 96.0 }
    }
  ]
}
```

---

## Milestones

Work one at a time. Build, run, and confirm the done-when criteria before proceeding.

### M0 — Window + Theme + Logging

Build: CMake project with all four submodules linked, Firefly initialised as the first
thing in `main`, `DawnTheme` + `DawnResolvers` defined, main window showing the Dawn
layout shell (left strip, central `ViewportWidget` placeholder, right strip), title label
reading the project name from the `.dawn` file via Amanuensis.

**Done when:** `./dawn data/test.dawn` opens a window with the amber dark Dawn aesthetic.
The title shows "Dawn PoC". `LOG_INFO` confirms the project file loaded. No blue Penumbra
demo colours anywhere.

---

### M1 — Serialisation + Tests

Build: `SceneDocument`, `SceneCommand` + stack + `CompoundCommand`, UUID v4 utility,
`SceneSerialiser` using Amanuensis for all three file formats. The `dawn_tests` binary
with Cimmerian covering the round-trip and basic command tests.

**Done when:** `./build/dawn_tests` passes — scene JSON round-trips without data loss,
`PlaceEntityCommand` adds an entity and undoes cleanly. `dawn` prints the entity count
from `test_scene.json` to the Firefly log on load.

---

### M2 — Viewport + Entities as Coloured Rectangles

Build: `ViewportWidget` wired in as the central pane, `OnRenderScene` drawing entities as
coloured rectangles at their world positions, camera pan (middle mouse drag) and zoom
(scroll wheel, centred on cursor) via `OnSceneInput`, entity list in the left panel as a
`ScrollablePanel` of Labels.

**Done when:** Open the test scene, see two coloured rectangles in the viewport at their
world positions. Pan with middle mouse, zoom with scroll wheel. Entity names appear in the
left panel list. Firefly logs viewport resize events.

---

### M3 — Placement + Selection + Movement + Properties

Build: entity palette (one `Button` per type from schema), placement mode (click canvas →
`PlaceEntityCommand`), click to select (accent-coloured outline in viewport), drag to move
(`MoveEntityCommand`), properties panel showing name (`TextInput`) and position x/y
(`NumericDrag`), Ctrl+Z / Ctrl+Shift+Z undo/redo. Add placement and move tests to
`dawn_tests`.

**Done when:** Place a platform, drag it, rename it in the properties panel, undo all
three step by step. Selection outline is visible. Firefly logs each command execution.

---

### M4 — Save / Load / Dirty State

Build: Ctrl+S saves scene JSON via Amanuensis `WriteToFile`, Ctrl+R reloads with a dirty
confirmation `Label`-based prompt, dirty `*` in the title area, graceful error display
(`Label` in the UI, `LOG_ERROR` to Firefly) for missing or malformed JSON — never crashes.

**Done when:** Full session — open, place entities, move them, Ctrl+S, verify the JSON on
disk, restart, open again, see the saved state. The `*` appears and disappears correctly.
Firefly logs save success and any IO errors.

---

## Working Method

- One milestone at a time. Build, run, confirm done-when, then proceed.
- Run `dawn_tests` after M1 and after any change to `SceneDocument` or `SceneSerialiser`.
- Ask questions rather than guessing — especially about Penumbra widget behaviour.
- Do not add features not listed above. If something seems missing, ask.
- The Dawn docs in `fearless-hq` are the full vision; this brief is the scope.
- No SDL calls in Dawn UI code. The one intentional exception: if sprite loading is ever
  added (not in scope here), `Renderer.GetSdlRenderer()` is available for texture upload.
