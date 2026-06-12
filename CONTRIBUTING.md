# Contributing to Forge

Thanks for your interest in Forge. This guide is the contract for code in this repository —
read it before opening a pull request. The goal is a codebase that reads as if one person
wrote it: **consistency beats personal preference.** When in doubt, open a neighbouring file
and copy its shape.

Forge is a C++20 / OpenGL 4.6 real-time engine (the `forge` static library) plus an integrated
ImGui editor (the `ForgeEditor` executable). What we are building lives in
[`docs/PLAN.md`](docs/PLAN.md); *how* we write it lives here.

---

## Getting set up

Requirements (Windows 11 x64): MinGW-w64 GCC 13.2, CMake ≥ 3.24, Ninja 1.12, Git.

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release
.\build\release\bin\ForgeEditor.exe
# or the one-liner:  .\run.ps1   (debug:  .\run.ps1 -Config debug)
```

All third-party dependencies are fetched and built automatically by `FetchContent` on first
configure — nothing to install by hand.

## Style guides — the C++ landscape

C++ has **no single official style guide** the way Python has PEP 8. Contributors should be
familiar with these, in order of authority for this project:

1. **This document + the house style below.** It wins for anything Forge-specific (the `m_`
   member prefix, `PascalCase` methods, the no-exceptions rule).
2. **[ISO C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)** — the closest
   thing to an authoritative *semantic* standard (RAII, ownership, const-correctness, rule of
   five). Follow these for *how to use the language*.
3. **Formatting** follows our `clang-format` config. Popular references behind common house
   styles are the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
   and the [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html) — useful context,
   but where they conflict with this file, **this file wins** (e.g. Google bans the `m_` prefix;
   Forge requires it).

**Run `clang-format` before committing.** Formatting is mechanical and not up for debate in
review — let the tool settle it. (If `.clang-format` is absent, match the surrounding file
exactly: 4-space indent, braces as in existing code, no trailing whitespace.)

## House style (authoritative)

### File conventions
- `#pragma once` at the top of every header — no include guards.
- Everything lives in `namespace forge { … }`, closed with a `// namespace forge` comment.
- **Include order**, blank-line separated: (1) the matching header, in a `.cpp`; (2) other
  `forge/…` headers; (3) third-party (`<GL/glew.h>`, GLFW, GLM, ImGui); (4) the C++ standard library.
- Includes are rooted at the library `src` dir: `#include "forge/renderer/Mesh.h"`. No
  cross-module `../../` relative includes. A `.cpp` may include its sibling header by bare name.

### Naming (the PEP-8-equivalent table for Forge)
| Kind | Style | Example |
|---|---|---|
| Types — class / struct / enum | `PascalCase` | `PathTracer`, `RaycastHit` |
| Methods / free functions | `PascalCase` | `CreateEntity`, `WorldTransform` |
| Member variables | `m_PascalCase` | `m_Program`, `m_Entities` |
| Locals / parameters | `camelCase` | `vertexPath`, `worldRay` |
| Constants / macros / compile defs | `FORGE_UPPER_SNAKE` | `FORGE_ASSERT`, `FORGE_ASSET_DIR` |
| Namespace | lowercase | `forge` |

### Class structure & OOP
- **`struct` = data, `class` = behaviour + invariant.** Components and POD records
  (`TransformComponent`, `Entity`, `RaycastHit`) are `struct`s with public, brace-initialised
  fields. Resource owners and types with invariants (`Shader`, `Mesh`, `Scene`) are `class`es:
  `public:` API first, `private:` state last, members initialised in-class.
- `explicit` on every single-argument constructor that isn't an intended implicit conversion.
- **Const-correctness is mandatory** — query methods are `const`, read-only args are passed by
  `const&`, provide `const` overloads for read paths.
- **No deep inheritance.** Prefer composition and free functions. Introduce `virtual` only at a
  real runtime-polymorphism seam, and always give such a base a `virtual ~Base() = default;`.
- File-local helpers are `static` free functions at the top of the `.cpp`, not private methods,
  when they need no member state.

### Resource management & ownership (RAII)
- Every GPU/OS handle is owned by exactly one object. **Constructor acquires, destructor
  releases** — no manual `Init()`/`Shutdown()` lifetime pairs.
- **Resource owners are non-copyable** — delete the copy operations explicitly. If a type must
  be relocated, implement move and respect the **rule of five**. A copyable GL-handle wrapper is
  a double-free waiting to happen.
- Express ownership in the type: `std::unique_ptr<T>` (unique), `std::shared_ptr<T>` (shared
  asset), raw `T*` / `T&` (**non-owning** observer), `std::optional<T>` (optional result). A raw
  pointer never owns.
- Wrap third-party C handles (GLFW window, GL objects) so the rest of the engine never sees the
  raw C type.

## Logging is mandatory

**Logging is not optional — it is part of writing the feature.** Forge is developed by watching
its log and its window; code that runs silently is code we can't debug. Use the macros in
[`engine/src/forge/core/Log.h`](engine/src/forge/core/Log.h):

- `FORGE_INFO(...)` — lifecycle and one-shot milestones: subsystem init, asset loaded, scene
  saved, render mode switched. Enough to reconstruct *what happened* from a log alone.
- `FORGE_WARN(...)` — recoverable oddities: a missing optional texture, a clamped parameter, a
  slow path taken. Something a developer should notice but that doesn't stop the frame.
- `FORGE_ERROR(...)` — failures, to `stderr`. **Always include the offending path / id / GL
  info-log** so the message is actionable on its own (see the shader compile/link reporting in
  `Shader.cpp`). Never discard a GL error silently — check status, log the driver info-log.

Rules of thumb: log at subsystem boundaries (load/init/teardown), log every failure with
context, and don't spam per-frame inner loops (guard hot-path logging behind a debug flag).
New subsystems must emit at least an init line and error logging before they're considered done.

## Error handling

Forge **does not use C++ exceptions for its own control flow.** Two categories:

- **Bugs and unrecoverable failures → assert and abort.** `FORGE_ASSERT(cond, "msg %s", detail)`
  logs and `std::abort()`s. Use it for precondition/invariant violations and unrecoverable
  startup failures (a core shader fails to compile, an asset is missing at a path the code
  itself constructed). These are bugs, not runtime conditions to paper over.
- **Expected, recoverable outcomes → return a value the caller must handle.** Lookup that may
  miss → `T*` that can be `nullptr` (`Entity* Find(UUID)`). Fallible query with a payload →
  `std::optional<T>` (`std::optional<RaycastHit> Raycast(...)`). Success/fail, no payload →
  `bool` (`bool Remove(UUID)`). Never assert on things that legitimately happen at runtime.
- If a third-party API throws, catch it at the module boundary and translate into the convention
  above — don't let it propagate into engine call sites.

## Documentation style

- **Comments explain *why*, not *what*.** Record the design decision, trade-off, or constraint a
  future reader would otherwise have to re-derive. (See the `Entity`-is-copyable rationale in
  `Scene.h`.)
- **Trailing line comments** carry units, sentinel meanings, and ranges:
  `float intensity = 20.0f; // radiance at 1m`, `UUID parent = 0; // 0 = root`.
- A one-sentence header comment states a class's single responsibility. Group related public
  methods with a banner comment (`// --- hierarchy ----`). No Doxygen ceremony.
- Keep `docs/PLAN.md` in sync when an architectural decision changes.

## Branching workflow

`main` is always green — it must configure, build warning-clean, and launch at every commit.
Never commit directly to `main`; all work happens on a short-lived branch off the latest `main`
and lands through a pull request.

**Branch naming** — `<type>/<short-kebab-description>`, matching the commit/changelog vocabulary:

| Prefix | Use for | Example |
|---|---|---|
| `feature/` | new functionality | `feature/shadow-mapping` |
| `bugfix/` | a defect fix | `bugfix/bvh-leaf-overflow` |
| `refactor/` | behaviour-preserving cleanup | `refactor/renderer-submit-api` |
| `docs/` | documentation only | `docs/contributing-branching` |
| `chore/` | build, tooling, deps | `chore/bump-imgui` |

Keep it lowercase and hyphenated; reference a milestone or issue when relevant
(`feature/m5-path-tracer`).

**Start a branch** — always branch from an up-to-date `main`:

```powershell
git checkout main
git pull --ff-only origin main          # sync to remote first
git checkout -b feature/shadow-mapping  # or bugfix/<name>, refactor/<name>, ...
```

**While you work** — commit in small, compiling steps (see below). Keep the branch current with
`main` by rebasing, so history stays linear and easy to bisect:

```powershell
git fetch origin
git rebase origin/main                  # resolve conflicts, re-test, continue
```

**Open the PR** — push the branch and open a pull request into `main`:

```powershell
git push -u origin feature/shadow-mapping
```

Before requesting review: `clang-format` clean, configures + builds warning-clean, the editor
launches, and the new code logs at its boundaries (see *Logging is mandatory*). A PR that
doesn't build won't be reviewed. Delete the branch after it merges.

## Commits & pull requests

- Small, focused commits. Imperative subject ≤ ~72 chars ("Add directional shadow map"), with an
  optional body explaining *why*.
- **Never commit secrets or build output.** `.env`, `build/`, logs, and `*.ini` are gitignored —
  keep them that way.
- **Verify before you push:** configure + build warning-clean + the editor launches. A commit
  that doesn't compile breaks `git bisect` for everyone after you.
- Each milestone (see PLAN.md) should compile, run, and be usable on its own — no big-bang
  integration.

By contributing you agree your work is licensed under the project's [MIT License](LICENSE).
