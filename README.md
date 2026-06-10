# Dawn — 2D Scene Editor (Proof of Concept)

> ⚠️ **This is an AI-developed prototype, not the final application.**
> Dawn was built end-to-end by an AI agent (Claude) following the brief in
> [`docs/dawn_poc_brief.md`](docs/dawn_poc_brief.md). It is **throwaway-grade** — written
> to answer a design question, not to ship. Expect to learn from it and rewrite it, not to
> extend it as-is. The real Dawn is described in the wider Umbra design docs; this repo is
> only the scoped proof of concept.

---

## What this is

Dawn is a minimal **2D scene editor** — a desktop application where you place, select,
move, rename, and persist entities in a scene. This repository is a focused prototype that
exists to answer one question:

> **Can Dawn's UI, scene management, and serialisation work together on top of
> [Penumbra](https://github.com/DeanWilsonDev/penumbra-proto)?**

Penumbra is a retained-mode, SDL3-based UI framework (itself a proof of concept). Dawn is
the first real tool built *on* Penumbra, so this PoC is really a test of the whole
consumption model: does Penumbra hold up as the UI foundation for a genuine editor, and is
the layering — theme → resolvers → composed widget tree, with a render seam for scene
drawing — pleasant to build against?

The intent, straight from the brief, was to produce a small but real Dawn executable that:

- **Looks like Dawn** — its own amber-on-dark theme, not the Penumbra demo aesthetic.
- **Hosts a viewport** where entities are visible and interactive.
- **Reads and writes scene JSON** through the Amanuensis library.
- **Supports placement, selection, movement, and undo/redo.**
- **Proves the Penumbra → Dawn consumption model is workable.**

## What it does

Run it against a project file and you get a three-pane editor:

- **Entity palette** (left) — one button per entity type, loaded from a JSON schema.
- **Viewport** (centre) — entities drawn as coloured rectangles at their world positions,
  with a camera you can pan and zoom.
- **Properties panel** (right) — name and X/Y position of the selected entity.

Supported interactions:

- **Place** — pick a type in the palette, click in the viewport.
- **Select** — click an entity (or its row in the list); a selected entity gets an amber
  outline.
- **Move** — drag a selected entity; its X/Y update live.
- **Edit** — rename via a text field, nudge position via Blender-style drag fields.
- **Undo / redo** — every placement, move, and rename is a discrete, reversible step.
- **Save / reload** — write the scene back to JSON, with a dirty (`*`) indicator and a
  discard-confirmation prompt on reload.
- **Logging** — lifecycle, commands, and IO are logged throughout (console + CSV).

Entities are intentionally drawn as plain coloured rectangles — sprite/image rendering is
out of scope for this PoC.

### Controls

| Action | Input |
| --- | --- |
| Place entity | Click a palette button, then click in the viewport |
| Select entity | Left-click the entity (or its list row) |
| Move entity | Drag a selected entity |
| Pan camera | Middle-mouse drag |
| Zoom camera | Scroll wheel (centred on cursor) |
| Undo / Redo | `Ctrl/Cmd+Z` / `Ctrl/Cmd+Shift+Z` |
| Save scene | `Ctrl/Cmd+S` |
| Reload scene | `Ctrl/Cmd+R` (prompts `Y`/`N` if there are unsaved changes) |

## Architecture at a glance

Dawn sits on top of four first-party libraries, included as git submodules under `libs/`:

| Library | Role |
| --- | --- |
| [penumbra-proto](https://github.com/DeanWilsonDev/penumbra-proto) | UI framework (SDL3, retained-mode widgets, render-to-texture viewport) |
| [amanuensis](https://github.com/DeanWilsonDev/amanuensis) | JSON read/write |
| [firefly](https://github.com/DeanWilsonDev/firefly) | Logging |
| [cimmerian](https://github.com/DeanWilsonDev/cimmerian) | BDD-style testing |

Key design rules the prototype holds to:

- **Dawn never calls SDL directly.** Penumbra owns that boundary; Dawn only touches SDL
  types where Penumbra's public API exposes them.
- **The scene model is pure logic.** `SceneDocument`, the command/undo stack, and
  `SceneSerialiser` know nothing about UI, SDL, or files-on-disk — so they're unit-tested in
  isolation by `dawn_tests`, which links neither Penumbra nor SDL.
- **All JSON goes through Amanuensis; all logging through Firefly** from the first line of
  `main`.
- **The look lives in Dawn, not Penumbra.** A `Dawn::Theme` defines every colour and
  measurement; `DawnResolvers` pour those into Penumbra's style structs.

### Source layout

```
src/
  main.cpp              # entry: logging, load .dawn/schema/scene, hand off
  EditorApplication.*   # the Penumbra-facing boundary: window, renderer, widgets, loop
  DawnTheme.*           # the amber-on-dark palette and metrics
  DawnResolvers.*       # Dawn theme -> Penumbra style structs
  SceneDocument.*       # pure-logic scene model (entities, layers, dirty flag)
  SceneCommand.*        # command base, stack, compound, place/move/rename commands
  SceneSerialiser.*     # .dawn / schema / scene JSON read & write (defensive)
  UuidGenerator.*       # RFC-4122 v4 UUIDs
  DawnLog.h             # centralised Firefly include
tests/                  # Cimmerian tests for serialisation and commands
data/                   # bootstrap project, schema, and scene JSON
assets/                 # bundled font
docs/                   # the brief, decisions log, and upstream issues
libs/                   # the four dependency submodules
```

## Building & running

**Requirements:** CMake ≥ 3.24, a C++20 compiler, and SDL3 + SDL3_ttf. On macOS:

```bash
brew install sdl3 sdl3_ttf      # tested with sdl3 3.4.10, sdl3_ttf 3.2.2
```

**Clone with submodules:**

```bash
git submodule update --init --recursive
```

**Configure and build:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Run the editor** (takes a `.dawn` project file as its only argument):

```bash
./build/dawn data/test.dawn
```

**Run the tests** (pure-logic; no UI/SDL needed):

```bash
./build/dawn_tests
```

## Scope — what it deliberately does *not* do

Per the brief, the following are out of scope for this prototype: sprite/image rendering,
a layer-management UI (layers exist in the model and JSON but aren't editable), native file
dialogs, transform gizmos, draggable panel splitters, schema hot-reload, tab bars,
preferences persistence, and multi-select. These belong to the full application, not the
PoC.

## Documentation

- [`docs/dawn_poc_brief.md`](docs/dawn_poc_brief.md) — the authoritative scope this
  prototype was built to.
- [`docs/decisions-log.md`](docs/decisions-log.md) — design decisions, deviations from the
  brief, and findings, milestone by milestone.
- [`docs/upstream-issues.md`](docs/upstream-issues.md) — bugs and rough edges found in the
  dependency libraries while integrating them, written up so they can be fixed in their own
  repos.

## Status

All four planned milestones are complete: **M0** window + theme + logging, **M1**
serialisation + tests, **M2** viewport + entities + camera, **M3** placement + selection +
movement + properties + undo/redo, **M4** save / load / dirty state. The build is clean and
the test suite passes. From here the prototype has served its purpose — the next step is a
fresh implementation of the real Dawn, informed by what this one proved.
