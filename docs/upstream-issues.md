# Upstream Library Issues Found Building the Dawn PoC

These issues live in the dependency libraries, **not** in Dawn. They were hit while
integrating the four submodules during the Dawn PoC (M0–M4). Each entry is written so it
can be filed and fixed in that library's own repository: it pins the commit, gives a
reproduction, the root cause, and a concrete suggested fix. Dawn currently carries a local
workaround for each open item; the workaround can be removed once the upstream fix lands.

Submodule commits these were observed against:

| Library | Commit |
| --- | --- |
| penumbra-proto | `8f2682ebcd14fb52a8dcdf2b6b5ff5817e8fc585` |
| amanuensis | `62d61e6abb02be520c97b84944aeaea07c4bfd5f` |
| firefly | `d6f847c025e62f3f4a2f3e1ed494ae4ef43a3871` |
| cimmerian | `f76a38da5b2f6bb623a78e907a1570d1979d9ec6` |

Severity: **High** = blocks a build / silently wrong behaviour · **Medium** = papercut
needing a workaround · **Low** = cosmetic / hygiene.

---

## Firefly

### FF-1 — `FIREFLY_DEFAULT_LOGGER` is `#define`d without an include guard · High

- **File:** `include/firefly/log.hpp:7`
- **Current code:**
  ```cpp
  #define FIREFLY_DEFAULT_LOGGER "Client"
  ```
- **Symptom:** The documented usage — define `FIREFLY_DEFAULT_LOGGER` to your app's
  logger name *before* including the header — does not work. `log.hpp` unconditionally
  redefines the macro back to `"Client"`, so every `LOG_*` call routes to a logger named
  `"Client"`. If the app only registered its own logger (e.g. `"Dawn"`), logs go to an
  unregistered logger. The compiler also emits `-Wmacro-redefined`.
- **Reproduce:**
  ```cpp
  #define FIREFLY_DEFAULT_LOGGER "Dawn"
  #include <firefly/log.hpp>          // warning: "FIREFLY_DEFAULT_LOGGER" macro redefined
  // ...
  Firefly::LogRegistry::RegisterLogger("Dawn");
  LOG_INFO("hello");                  // actually targets "Client", not "Dawn"
  ```
- **Root cause:** Unguarded `#define`. Because the `LOG_*` macros expand
  `FIREFLY_DEFAULT_LOGGER` at the call site, the header's definition always wins over a
  pre-include definition.
- **Suggested fix:** guard the default so a pre-include definition survives:
  ```cpp
  #ifndef FIREFLY_DEFAULT_LOGGER
  #define FIREFLY_DEFAULT_LOGGER "Client"
  #endif
  ```
- **Dawn workaround:** `src/DawnLog.h` includes `log.hpp` and then `#undef` + re-`#define`s
  the macro to `"Dawn"` *after* the include. Every Dawn TU that logs includes `DawnLog.h`.

### FF-2 — Bundled Amanuensis is added with no target guard · High

- **File:** `CMakeLists.txt:38` — `add_subdirectory(external/amanuensis)`
- **Symptom:** A superbuild that adds Firefly *and* Amanuensis (Dawn does:
  `add_subdirectory(libs/amanuensis)` and `add_subdirectory(libs/firefly)`) fails to
  configure:
  ```
  add_library cannot create target "amanuensis" because another target with the same
  name already exists.
  ```
- **Reproduce:** in one CMake project, `add_subdirectory` both a standalone Amanuensis and
  Firefly (which vendors its own Amanuensis copy under `external/`).
- **Root cause:** Firefly always adds its vendored Amanuensis as a subdirectory, creating
  the `amanuensis` target unconditionally. The same pattern recurs across the ecosystem:
  Firefly and Amanuensis both vendor Cimmerian, and Firefly's vendored Amanuensis vendors
  Cimmerian again — every one of these is an unguarded bundled dependency waiting to clash.
- **Suggested fix:** guard every bundled-dependency add so a parent project can supply it:
  ```cmake
  if(NOT TARGET amanuensis)
    add_subdirectory(external/amanuensis)
  endif()
  ```
  Apply the same `if(NOT TARGET ...)` pattern to all `add_subdirectory(external/...)`
  calls in Firefly, Amanuensis, and Cimmerian. (Amanuensis already only adds its test deps
  when it is the root project, which is the right idea; extend that discipline to the
  unconditional adds.)
- **Dawn workaround:** let Firefly create the `amanuensis` target, and guard Dawn's own add
  with `if(NOT TARGET amanuensis)`. Safe only because both Amanuensis copies are pinned to
  the same commit (`62d61e6`); if they diverge, the vendored copy silently wins.

---

## Cimmerian

> CIM-1 below is a **bug**. For Cimmerian *feature* requests (missing `ASSERT_NEAR`,
> exception assertions, etc.) that would have improved test coverage, see the separate
> roadmap doc [`cimmerian-feature-requests.md`](cimmerian-feature-requests.md).

### CIM-1 — `IT` / `TEST` break on commas inside `{}` test bodies · Medium

- **File:** `include/cimmerian/test.hpp:92` — `#define IT(testName, BODY) TEST(testName, BODY)`
- **Symptom:** `IT` takes exactly two macro arguments. The C preprocessor only shields
  commas inside parentheses, **not** braces, so any brace-initialiser containing a comma in
  a test body splits into extra macro arguments and fails to compile:
  ```cpp
  IT("falls back", {
    Color fallback{1, 2, 3, 4};   // preprocessor sees 4 extra macro args
    // ...
  });
  // error: too many arguments provided to function-like macro invocation
  ```
- **Root cause:** `IT` (and `BEFORE_EACH`/`AFTER_EACH`/etc.) forward a single `BODY`
  parameter, but `TEST` is already variadic (`#define TEST(testName, ...)`). The fixed-arity
  wrappers re-introduce the comma fragility that `TEST`'s `...` was meant to solve.
- **Reproduce:** any test body with a top-level comma — aggregate init `T x{a, b};`, a
  multi-arg template `std::map<int, int> m;`, or a bare `int a, b;`.
- **Suggested fix:** make the wrappers variadic so they pass the body through unchanged:
  ```cpp
  #define IT(testName, ...)         TEST(testName, __VA_ARGS__)
  #define IT_FN(testName, FN)       TEST_FN(testName, FN)
  // and similarly for BEFORE_EACH / AFTER_EACH / BEFORE_ALL / AFTER_ALL if they
  // forward a single BODY parameter.
  ```
- **Dawn workaround:** avoid brace-with-commas in test bodies (set struct fields
  individually instead).

---

## Amanuensis

### AM-1 — Umbrella header path differs from documentation · Medium

- **File:** `include/amanuensis.hpp` (at the include root).
- **Symptom:** The documented include `#include <amanuensis/amanuensis.hpp>` fails with
  *"file not found"*. The real umbrella header is `<amanuensis.hpp>`.
- **Root cause:** Docs/examples reference a nested path that does not exist; the umbrella
  sits at the include root next to the `amanuensis/` directory.
- **Suggested fix:** pick one and make it true — either add a one-line forwarding header at
  `include/amanuensis/amanuensis.hpp`:
  ```cpp
  #pragma once
  #include <amanuensis.hpp>
  ```
  or update the docs/README to `#include <amanuensis.hpp>`. (A forwarding header is the
  more forgiving option since it makes both spellings work.)
- **Dawn workaround:** include `<amanuensis.hpp>` (umbrella) and, where only specific types
  are needed, the concrete headers `<amanuensis/value.hpp>`, `<amanuensis/io/reader.hpp>`,
  `<amanuensis/io/writer.hpp>`.

### AM-2 — Mixed British/American spelling in the serialisation headers · Low

- **Directory:** `include/amanuensis/serialisation/` (British `-isation`) contains
  `serialization.hpp` (American `-ization`) alongside `serialize.hpp` and
  `serialize-macros.hpp`.
- **Symptom:** Cosmetic but error-prone — the directory and file names disagree on spelling,
  making includes easy to mistype. The rest of the public API (`SceneSerialiser`-style
  usage, the `serialisation/` directory) leans British.
- **Root cause:** Inconsistent naming convention across files added at different times.
- **Suggested fix:** choose one spelling and apply it consistently to the directory and all
  file names (and any referenced include paths). If renaming public headers is a breaking
  change, ship forwarding headers under the old names for one release.
- **Dawn workaround:** none needed — Dawn builds scene JSON manually via the `Value` API and
  does not include these headers directly.

---

## Penumbra

No issues found across M0–M4. The widget/render/input/viewport API matched
`docs/penumbra_poc_spec.md` and the demo. One behaviour worth documenting (not a bug):
`Box` stacking lays out each child at its *measured* size — there is no flex/grow — so a
consumer that wants a region to fill remaining space computes rects itself and arranges
top-level widgets explicitly (exactly what the demo and Dawn do). A note in the spec or a
future `Flex`/`Spacer` affordance would save the next consumer the discovery.

---

## Quick checklist to file

- [ ] **Firefly FF-1** — guard `FIREFLY_DEFAULT_LOGGER` with `#ifndef` (High)
- [ ] **Firefly FF-2** — `if(NOT TARGET ...)` around bundled `add_subdirectory`s (High)
- [ ] **Cimmerian CIM-1** — make `IT`/hook macros variadic (Medium)
- [ ] **Amanuensis AM-1** — forwarding umbrella header or doc fix (Medium)
- [ ] **Amanuensis AM-2** — normalise serialisation spelling (Low)
