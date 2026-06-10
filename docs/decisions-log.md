# Dawn PoC — Decisions & Findings Log

A running record of the decisions made while building the Dawn PoC and the issues
discovered along the way. It exists for two reasons:

1. **Cross-project fixes** — the Dawn PoC consumes Penumbra, Amanuensis, Firefly, and
   Cimmerian as submodules. Building against them surfaced concrete bugs and rough
   edges in those libraries. Those are collected in [§1](#1-upstream-library-issues)
   so they can be tracked and fixed in their own repos.
2. **Dawn's own decisions** — deviations from `dawn_poc_brief.md` and internal design
   choices, so the next person (or session) knows what changed and why.

Status legend: 🔴 open · 🟡 worked around in Dawn · 🟢 fixed upstream

Last updated: 2026-06-10 (after M2).

---

## 1. Upstream library issues

These are **not** Dawn bugs. Each was hit while integrating a dependency and should be
fixed in that library's repository. Dawn carries a local workaround so the PoC can
proceed; the workaround should be removed once the upstream fix lands.

### 1.1 — Firefly: `FIREFLY_DEFAULT_LOGGER` redefine is unguarded 🟡

- **Repo:** `firefly`
- **Location:** `include/firefly/log.hpp:7`
- **Symptom:** `log.hpp` does `#define FIREFLY_DEFAULT_LOGGER "Client"` with no
  `#ifndef` guard. The documented pattern — `#define FIREFLY_DEFAULT_LOGGER "Dawn"`
  *before* the include — is silently overridden, so all `LOG_*` calls target the
  unregistered `"Client"` logger instead of the app's logger. Compiler emits
  `-Wmacro-redefined`; runtime logs go to the wrong (or a missing) logger.
- **Dawn workaround:** `src/DawnLog.h` includes `log.hpp` and *then* `#undef` +
  `#define FIREFLY_DEFAULT_LOGGER "Dawn"`. The `LOG_*` macros expand the token at the
  call site, so a post-include redefinition takes effect. Every Dawn `.cpp` that logs
  includes `DawnLog.h` instead of `firefly/log.hpp`.
- **Suggested upstream fix:** guard the default:
  ```cpp
  #ifndef FIREFLY_DEFAULT_LOGGER
  #define FIREFLY_DEFAULT_LOGGER "Client"
  #endif
  ```
  This makes the "define before include" pattern in the docs actually work.

### 1.2 — Firefly bundles Amanuensis and adds it unconditionally 🟡

- **Repo:** `firefly`
- **Location:** `CMakeLists.txt` — `add_subdirectory(external/amanuensis)` with no
  target guard.
- **Symptom:** In a superbuild that also adds Amanuensis (as the Dawn brief's CMake
  does via `add_subdirectory(libs/amanuensis)`), CMake aborts with
  *"add_library cannot create target 'amanuensis' because another target with the same
  name already exists."*
- **Dawn workaround:** let Firefly create the `amanuensis` target, and guard Dawn's own
  add: `if(NOT TARGET amanuensis) add_subdirectory(libs/amanuensis) endif()`. Safe here
  only because both copies are pinned to the **same commit** (`62d61e6`); if they ever
  diverge, the bundled copy silently wins.
- **Suggested upstream fix:** guard bundled-dependency adds so a parent project can
  provide the dependency:
  ```cmake
  if(NOT TARGET amanuensis)
    add_subdirectory(external/amanuensis)
  endif()
  ```
  The same guard belongs on every `add_subdirectory(external/...)` in the ecosystem
  (Amanuensis and Firefly both bundle Cimmerian; Firefly's nested Amanuensis bundles
  Cimmerian again).

### 1.3 — Cimmerian: `IT` / `TEST` break on commas inside `{}` 🟡

- **Repo:** `cimmerian`
- **Location:** `include/cimmerian/test.hpp` — `#define IT(testName, BODY) TEST(testName, BODY)`
- **Symptom:** `IT` takes exactly two macro arguments. The C preprocessor only shields
  commas inside parentheses, **not** braces, so any brace-init with a comma in a test
  body splits into extra arguments:
  ```cpp
  IT("falls back", {
    Dawn::Color Fallback{1, 2, 3, 4}; // <-- preprocessor sees 4 extra args
    ...
  });
  ```
  Fails with *"too many arguments provided to function-like macro invocation."* Easy to
  hit with aggregate initialisation and confusing to diagnose.
- **Dawn workaround:** avoid brace-with-commas in test bodies — assign struct fields
  individually instead.
- **Suggested upstream fix:** make `IT` variadic, mirroring `TEST` (which already is):
  ```cpp
  #define IT(testName, ...) TEST(testName, __VA_ARGS__)
  ```
  This lets test bodies contain top-level commas (brace-init, multi-arg templates).

### 1.4 — Amanuensis: umbrella header path differs from the documented one 🟡

- **Repo:** `amanuensis` (also a `dawn_poc_brief.md` doc bug — see §2.1)
- **Location:** `include/amanuensis.hpp` (at the include root).
- **Symptom:** The brief and inline docs show `#include <amanuensis/amanuensis.hpp>`,
  but the umbrella header is actually `<amanuensis.hpp>`. The documented include fails
  with *"file not found."*
- **Dawn workaround:** include `<amanuensis.hpp>` (umbrella) and the specific headers
  `<amanuensis/value.hpp>`, `<amanuensis/io/reader.hpp>`, `<amanuensis/io/writer.hpp>`.
- **Suggested upstream fix:** either add a forwarding header at
  `include/amanuensis/amanuensis.hpp`, or correct the documentation to `<amanuensis.hpp>`.

### 1.5 — Amanuensis: British/American spelling is mixed 🔴

- **Repo:** `amanuensis`
- **Location:** `include/amanuensis/serialisation/` (British) contains
  `serialization.hpp` (American) alongside `serialize.hpp` / `serialize-macros.hpp`.
- **Symptom:** Cosmetic but error-prone — the directory is `serialisation/` while files
  inside use the American `-ize/-ization`. Easy to mis-type includes. Not blocking;
  Dawn does not include these directly yet (it uses manual `Value` building).
- **Dawn workaround:** none needed yet.
- **Suggested upstream fix:** pick one spelling and apply it consistently to the
  directory and file names.

### Penumbra

No blocking issues found in M0–M1. The widget/render/input API matched
`penumbra_poc_spec.md` and the demo. `ViewportWidget`, `Box` layout, and `InputState`
behaved as documented (notably: `Box` stacking has no flex/grow — see §3.2). Findings
from M2+ (viewport wiring, camera) will be added here.

---

## 2. `dawn_poc_brief.md` inaccuracies

The brief is the authoritative scope, but a few specifics didn't match the real
libraries. Flagged so the brief can be corrected for the next session.

- **§2.1 Amanuensis include** — brief says `<amanuensis/amanuensis.hpp>`; actual is
  `<amanuensis.hpp>`. (See §1.4.)
- **§2.2 Firefly logger define** — brief says define `FIREFLY_DEFAULT_LOGGER` *before*
  the include; that does not work with the current header. (See §1.1.)
- **§2.3 `dawn_tests` CMake** — the brief's sample `add_executable(dawn_tests ...)`
  lists only the `tests/*.cpp` files, so it would not link the implementations under
  test (`SceneSerialiser`, `SceneDocument`, commands). Dawn factors a
  `DAWN_CORE_SOURCES` list and compiles it into both `dawn` and `dawn_tests`.
- **§2.4 Submodule independence** — the brief's CMake assumes each `libs/*` is
  independent, but Firefly (and Amanuensis) bundle their own copies of shared deps,
  causing target-name collisions. (See §1.2.)
- **§2.5 Scene selection** — the bootstrap `test.dawn` names no scene file, so it is
  ambiguous which scene `dawn` should open. Dawn opens `<scenesRoot>/test_scene.json`
  by convention. A `defaultScene` field in the `.dawn` file would remove the guess.
- **§2.6 Entity editor colour** — the brief's sample camera code reads
  `Entity.EditorColor` (an `SDL_Color` on the entity). Storing an SDL type on the
  entity would pull SDL into the pure-logic layer and the SDL-free `dawn_tests`. Dawn
  instead resolves colour by entity *type* from the schema at render time. (See §3.1.)
- **§2.7 Initial camera vs. "see the entities on open"** — the brief's camera defaults
  to `{Offset 0, Zoom 1}`, but the test scene's entities sit at world y≈700–832 while
  the viewport is only ~740px tall, so they fall off the bottom and the M2 done-when
  ("see two coloured rectangles on open") would not be met. Dawn adds a one-time
  frame-on-open. (See §3.7.)

---

## 3. Dawn design decisions

Internal choices not dictated by the brief.

### 3.1 — Entity colour is resolved by type, not stored on the entity

`EntityData` holds no colour. Colour is a property of the entity *type* (from the
schema) and is looked up at render time in `EditorApplication`. This keeps
`SceneDocument`/`EntityData` free of `SDL_Color` so they compile into the SDL-free
`dawn_tests` binary, and avoids the serialiser needing the schema to fill a colour
field. A neutral `Dawn::Color` (plain RGBA) carries schema colours through the
pure-logic layer; conversion to `SDL_Color` happens only at the Penumbra boundary.

### 3.2 — Manual three-column layout instead of a flex container

Penumbra's `Box` stacking lays out each child at its *measured* size — there is no
flex/grow. So Dawn (like the Penumbra demo) computes the header/left/viewport/right
rects itself each frame and arranges each top-level widget into an explicit rect.
This is the intended consumption model and means window resize "just works."

### 3.3 — Pure-logic core is Firefly-free; logging lives in the caller

To honour both "Firefly logging throughout" and "`dawn_tests` must not link Firefly,"
the pure-logic modules (`SceneDocument`, `SceneCommand`, `SceneSerialiser`,
`UuidGenerator`) contain no logging. They return status/`bool`, and the caller
(`main` / `EditorApplication`, which own Firefly) logs the outcome. Serialiser methods
are defensive: missing or wrong-typed JSON fields fall back to defaults rather than
throw, so malformed input degrades gracefully.

### 3.4 — `EditorApplication` owns the Penumbra boundary; `main` stays thin

`main` does logging setup, reads the `.dawn`/schema/scene via Amanuensis +
`SceneSerialiser`, then hands the loaded `ProjectData`/`EntitySchema`/`SceneDocument`
to `EditorApplication`, which owns the window, renderer, font backend, widget tree, and
frame loop. This is the single place that touches Penumbra's window/render layers,
keeping "Dawn never calls SDL directly" in one file.

### 3.5 — `DawnLog.h` centralises the Firefly include

All Dawn logging goes through `src/DawnLog.h` rather than `firefly/log.hpp` directly,
so the §1.1 workaround lives in exactly one place and can be deleted in one place once
Firefly is fixed.

### 3.6 — CMake grows per milestone

`CMakeLists.txt` only compiles the sources a milestone needs, so the project always
builds and runs at each checkpoint. It converges on the brief's final CMake by M4.

### 3.7 — Frame-on-open (one-time camera fit)

When the scene opens, the camera centres the entities' bounding box in the viewport at
a zoom that fits with an 0.8 margin (clamped to the same 0.1–10 range as wheel zoom).
This runs once, on the first frame the content size is known. Without it the default
`{0,0,1}` camera leaves the test scene's entities off the bottom of the viewport, so
this is what makes the M2 done-when verifiable. Pan/zoom from the brief are otherwise
untouched. Easy to remove if the brief prefers the raw default camera. (See §2.7.)

### 3.8 — Firefly log-level conventions

Settled on a consistent mapping (logger registered with debug logging on, so TRACE/
DEBUG are visible):

- **INFO** — lifecycle: startup, project/schema/scene load, window open, shutdown.
- **DEBUG** — diagnostic one-offs: frame-on-open result, (M3) command execution.
- **TRACE** — high-frequency events: viewport resize.
- **WARNING** — missing-but-recoverable data: no schema, missing/invalid scene → empty doc.
- **ERROR** — parse/IO failures that abort an operation.

### 3.9 — UI and camera state currently live as `Run()` locals

The widget tree, `CameraState`, and `LastMousePos` are locals inside
`EditorApplication::Run()`, captured by reference into the viewport callbacks (the
callbacks live as long as the widgets, which live as long as `Run()`). The entity-list
panel is likewise built once from the loaded scene — **static in M2**. M3 introduces
selection, a command stack, and placement that mutates the entity list, so this state
will be promoted to `EditorApplication` members and the list rebuilt on change. Noted
so the M2→M3 refactor is expected, not a surprise.

A known, PoC-accepted limitation: `LastMousePos` updates only while the cursor is over
the viewport, so starting a middle-drag the instant the cursor re-enters can produce a
one-frame pan jump. Matches the brief's sample and is not worth fixing for the PoC.

---

## 4. Tooling notes

### 4.1 — Editor/LSP reports phantom missing-include errors

There is no `compile_commands.json` at the repo root, so clangd-based editor
diagnostics can't resolve the submodule include paths and flag false positives like
*"'firefly/log.hpp' file not found"* or *"Unknown type name 'SDL_Color'"* in
`EditorApplication.cpp`. **These are not real** — the CMake build compiles and links
cleanly. If editor diagnostics are wanted, point the LSP at the generated
`build/compile_commands.json` (CMake writes one; symlink it to the repo root). Don't
chase these as build errors.
