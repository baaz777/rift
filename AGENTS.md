# Repository instructions

## Rules

1. Ask when unclear. If intent, architecture, or requirements are ambiguous, ask before coding.
2. Flag uncertainty. If an approach, dependency, or technical detail is uncertain, say so before proceeding.
3. Challenge bad direction. If my request conflicts with settled practice or likely long-term maintainability, point it out and suggest a better path.
4. End with omissions. After each task, state what you changed and what you intentionally did not do.

## Documentation

Four rules everywhere in comments and docstrings.

1. What, why, how, in that order, and only as much as is needed. In prose, never restate the signature; add units, ranges and nullability, or say nothing.
2. Sentence case. Capitalize sentence starts, command descriptions, and section titles after icons. Preserve the case of commands, identifiers, URLs, and icon shortcodes.
3. No archaeology. Do not write prose about the history of a class throughout the life cycle of this repository or what an earlier implementation did.
4. Concise. Short sentences, active voice, one idea each, one term per concept per file. A set of cases wants a table; a flow wants a diagram.

Document the code using ASD-STE100-inspired Simplified Technical English: use short, direct sentences, one term per concept, active voice, explicit conditions, and avoid idioms, unnecessary synonyms, or ambiguous wording. Focus documentation on intent, constraints, side effects, and non-obvious behavior;
Write for an engineer who knows the language but not this system.
Read [CONTRIBUTING.md](CONTRIBUTING.md) before writing or reviewing code documentation.

## Project map

Rift is a Windows C++23 RPG with a tile-based world, a built-in editor, and OpenGL/Vulkan backends
that can switch at runtime. Both flat 2.5D and world-space 3D rendering paths exist.

- `src/` is flat. `main.cpp` starts `Game`; `GameInput`, `GameMenus`, and `GameDialogue` split its
  implementation. `EntityStore`, component headers, and `*System` files implement actors.
- `Tilemap`, collision/navigation helpers, and `SurfaceSystem` implement world behavior.
  `Editor*`, `EditorCommands`, and `UndoRedoStack` implement editing and history.
- `IRenderer`, `OpenGLRenderer`, `VulkanRenderer*`, and `shaders/` implement graphics.
  `TimeManager`, `Weather*`, `SkyRenderer`, and `ParticleSystem` implement environmental effects.
- `tests/` contains GoogleTest tests, `MockRenderer.hpp`, and link-only `TestStubs.cpp`.
- `rift.project.json` supplies asset paths and project settings through `ProjectManifest`.
- `docs/` contains subsystem guides; `Doxyfile.in` configures generated API documentation.
  `src/Version.hpp` supplies the version parsed by CMake.

Start with [architecture](docs/ARCHITECTURE.md), then the relevant
[rendering](docs/RENDERING.md), [collision](docs/COLLISION.md), [editor](docs/EDITOR.md), or
[project manifest](docs/PROJECT_MANIFEST.md) guide.

## Build, test, and run

Run commands from the repository root. The standard setup uses Windows x64, Visual Studio 2022
with C++ tools, CMake 3.21+, the Vulkan SDK, and vcpkg. Set `VCPKG_ROOT` before using the `default`
preset; it uses the `x64-windows` triplet. Both graphics APIs are build dependencies, even for
tests. See [setup](docs/SETUP.md) and [building](docs/BUILDING.md) for dependency details.

```powershell
cmake --preset default
cmake --build build --config Release --target rift
cmake --build --preset tests-release
ctest --preset tests-release
```

For a focused test after building `rift_tests`:

```powershell
.\build\Release\rift_tests.exe --gtest_filter="SuiteName.TestName"
```

- `BUILD_TESTS` defaults to `ON`; game and tests share `build/`. Specify a target to limit a build.
- Rerun configure after adding source or test files: CMake uses globs without `CONFIGURE_DEPENDS`.
  Production sources needed by tests may also require an entry in `TEST_LIB_SOURCES`.
- `setup.ps1` downloads dependencies and can update existing GLFW/GLM checkouts. Use it when
  setup is needed, rather than as a routine check.
- `build.bat` formats all top-level source files in place, configures, runs static analysis,
  builds the game in Debug and Release, and runs Doxygen. It does not execute tests.
  `--skip-tidy` still formats sources; prefer focused commands for small changes.
- `test.bat` builds Release tests and runs the GoogleTest executable directly.
- `run.bat` starts the Release game with the repository root as its working directory. Valid
  local assets are required to run. Configure paths in `rift.project.json` instead of `Game.cpp`.
  The game build also copies `assets/`; a checkout without assets may need an empty directory
  for the build step, as CI uses.

For targeted formatting, run `clang-format -i` on the changed C++ files. For static analysis,
configure `cmake --preset compile-db` in a compiler environment with Ninja available, then run
`clang-tidy -p build-cdb src/ChangedFile.cpp`. `build-cdb/` is a configure-only sidecar for
clang-tidy/clangd and shares vcpkg packages with `build/`; do not build it as a second game tree.

After configuring, generate documentation with `doxygen build\Doxyfile`. Edit `Doxyfile.in`,
not the generated configuration; output goes to `docs/html/`.

## Architecture contracts

- `Game` owns the EnTT registry. Keep components as plain data and behavior in systems; use
  `EntityStore` for actor creation and queries. `WorldServices` holds borrowed, nullable service
  pointers in the registry context. Handle missing services where the API permits them.
- Preserve NPC `Identity::instanceId` in snapshots and undo records. Entity handles are runtime
  references, not persistent identity.
- Renderer interface changes must update `RIFT_DECLARE_COMMON_RENDERER_METHODS`, both backends,
  and `tests/MockRenderer.hpp`. Preserve both flat and 3D behavior. Backend switches recreate
  renderer/window objects; cached pointers become invalid and GPU resources need rebuilding.
- Actor positions use bottom-center feet anchors in world pixels. Reuse coordinate and scene
  helpers. Query `Tilemap::GetLayerCount()` rather than hard-coding the default layer count.
- Persisted tile changes need initialization/reset, save/load support, undo data, and relevant
  editor updates. Preserve sparse serialization and compatibility with existing maps.
- `UndoRedoStack::Execute` applies a command; `Push` records an already-applied change with its
  complete undo state. Commands must not retain references to temporary `EditorContext` objects.
- `TextureStore` owns textures; handles belong to their originating store. Reuse procedural
  handles across reloads: `Acquire` deduplicates paths, while `Adopt` adds a new entry.

## Validation

Add or update meaningful tests for non-trivial behavior changes, especially math, serialization,
state transitions, and reproducible bugs. Tests link graphics libraries but do not create a GPU
context; use `MockRenderer` for draw assertions. Inspect `TestStubs.cpp` before assuming an
editor or game method executes real behavior in tests.

Build affected targets and run relevant tests; use the full suite for shared-system changes.
Check both backends and render paths when graphics changes require manual validation. For
documentation-only edits, verify references and formatting without an unnecessary game build.
Report what changed, the checks actually run, and any checks blocked by tooling or missing assets.
