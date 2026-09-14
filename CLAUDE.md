# CLAUDE.md

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

Rift is a 2.5D tile-based RPG in C++23 with runtime-switchable OpenGL 4.6 and Vulkan 1.4 backends,
an EnTT ECS world, a day/night + weather simulation, and an in-game level editor. Windows/MSVC only.

## Environment prerequisites

- **`VCPKG_ROOT` must be set.** The `default` and `base-vcpkg` presets are guarded by a
  `notEquals ""` condition on it; unset, the preset is *disabled* and configure fails immediately.
- **Vulkan SDK is a hard dependency.** `find_package(Vulkan REQUIRED)` — both backends are always
  compiled in, chosen at runtime. Never add `#ifdef USE_VULKAN` guards; that macro is vestigial.
- **`.\setup.ps1`** (one-time) clones GLFW + GLM into `external/` and downloads `nlohmann/json.hpp`
  and the pinned EnTT v4 header (`85c6bba…`). GLAD and stb are vendored in-repo.
- **`assets/` is gitignored and not distributed.** Without it the manifest fails tileset/sprite
  validation and startup aborts. CI creates empty `assets/`/`shaders/` dirs purely to make the
  build link.

## Commands

```powershell
.\build.bat                # full gate: clang-format -> configure -> clang-tidy -> Debug+Release -> Doxygen
.\build.bat --skip-tidy    # same without the blocking static-analysis step
.\test.bat                 # configure + build rift_tests + run them
.\run.bat                  # runs build\Release\rift.exe from the repo root

cmake --preset default
cmake --build build --config Release --target rift        # game only
cmake --build build --config Release --target rift_tests  # tests only
ctest --test-dir build -C Release --output-on-failure
```

Single test / subset:

```powershell
build\Release\rift_tests.exe --gtest_filter=TimeManagerTests.*
ctest --test-dir build -C Release -R "CollisionSystem" --output-on-failure
```

Notes:

- **`build.bat` step 3 is the real clang-tidy gate.** It configures the `compile-db` Ninja sidecar
  into `build-cdb/` and runs tidy per-TU against that database. The standalone `cmake --target tidy`
  points at the MSBuild tree, lacks the sidecar's C++23/include flags, and emits phantom errors —
  do not use it to validate.
- **`build.bat` step 1 rewrites `src/` in place.** Commit or stash first if you want formatting as a
  separate change. CI enforces clang-format over `src/` only; `tests/` is never format-checked.
- **Game and tests share one `build/` tree** (`BUILD_TESTS=ON` by default), so a bare
  `cmake --build build` builds both and a test compile error fails an apparently game-only build.
- Shader → SPIR-V compilation, asset/shader copies and the `rift.project.json` copy are CMake
  post-build steps, not script steps. `*.spv` is gitignored on purpose: it is a build artifact,
  never a committed fallback.
- Runtime resolves `rift.project.json`, `assets/` and `shaders/` **relative to the working
  directory**, which is why `run.bat` cds to the repo root before launching.
- The version string is parsed by CMake out of `src/Version.hpp`; bump it there.

## Architecture

### Composition root

`Game` owns every subsystem **by value** (tilemap, renderer `unique_ptr`, texture/dialogue/asset
stores, time, sky, weather, particles, editor, console, camera) plus the `entt::registry m_World`.
Its implementation is split into partials by concern: `Game.cpp` (lifecycle + frame loop),
`GameInput.cpp` (input routing), `GameMenus.cpp` (title/pause menus and the 3D render path),
`GameDialogue.cpp` (dialogue presentation).

Frame order is `poll → clamp dt → ProcessInput → Update → Render → FPS limiter`. Polling precedes
input because GLFW only refreshes cached key state on poll; the `MAX_DELTA_TIME` clamp is what stops
a debugger pause from teleporting actors through walls.

### ECS

There are no `Player`/`NPC` classes. An entity is a set of plain-struct components in `m_World`;
behavior is **stateless free functions** over the registry (`PlayerSystem`, `PlayerMovementSystem`,
`MotionSystem`, `NpcAiSystem`, `CollisionSystem`, `SurfaceSystem`, `CharacterKinematics`).
`EntityStore` is the only spawn/despawn/query seam.

Shared services are *not* components. `Game` publishes a `WorldServices` bundle of **non-owning,
all-nullable** pointers into `m_World.ctx()` — that is how a stateless system reaches a texture
store or dialogue tree. Tests routinely publish a partial bundle, so every reader must null-check.

Deliberate asymmetries: the player carries no `Identity` (it is reached via `Game::m_PlayerEntity`);
NPCs carry no `Hitbox` (dimensions come from `CharacterConstants`).

### Rendering

`IRenderer` is a strategy interface with `OpenGLRenderer` and `VulkanRenderer` behind
`RendererFactory`. **Declare backend overrides with `RIFT_DECLARE_COMMON_RENDERER_METHODS`
(`RendererMacros.hpp`), never by hand** — that macro plus `IRenderer` is the single place a shared
signature changes. A runtime `renderer.set` destroys and recreates the window *and* the renderer,
then re-uploads every texture through `TextureStore`; anything caching an `IRenderer*` dangles.

Two paths coexist: the default flat 2.5D Y-sorted path, and a world-space 3D path behind the
`world3d` console toggle (`CameraRig`, `Billboard`, `Frustum`, `SceneMath`, `Geometry3D.*` shaders).
World-space draws carry an explicit `renderModes::{BlendMode, DepthMode, LightMode}` triple —
`RenderModes.hpp` documents which pass uses which combination and why.

### World

`Tilemap` is a **dynamic** stack of layers plus per-cell grids. The default stack is 10 layers, but
`LoadMapFromJSON` rebuilds it from the map's `dynamicLayers[]`: always call `GetLayerCount()`, never
hard-code 10, and derive an actor's side from a layer's `isBackground` flag, not its index.

The per-layer/per-cell split is load-bearing: tiles, rotation, flips, stance, elevation role, y-sort
and structure ids are **per layer, per tile**; collision, navigation, elevation (px) and the
corner-cut mask are **per cell** (`BoolGrid`), belonging to no layer at all.

### Console and editor

~100 console commands (`F12`, `help`) are free functions taking a `CommandContext`. That struct is a
**single-frame, all-nullable, never-store** bundle rebuilt per invocation — the same contract
`EditorContext` carries. Commands are unit-tested by hand-building a partial context, so keep new
ones dependent only on the members they actually use.

Every editor mutation goes through an `EditorCommand` so `Ctrl+Z`/`Ctrl+Y` work; a new paintable
tile property needs storage in `Tilemap`, JSON round-tripping, a command, and an overlay in
`EditorRendering.cpp`.

## Traps specific to this repo

- **Adding a `src/*.cpp` that tests need requires editing `TEST_LIB_SOURCES` in `CMakeLists.txt`.**
  The test target globs `tests/*.cpp` but enumerates its `src/` dependencies by hand. Omitting the
  file gives link errors, not a compile error.
- **Tests link OpenGL/Vulkan but never create a context.** Drive draw paths through
  `tests/MockRenderer.hpp`; never call `LoadTextures` or other GPU-resource entry points from a
  test. `TestStubs.cpp` supplies link-only geometry stubs.
- **`EnumTraits<E>` specializations must keep `Count` and the `Names[]` array in step** — several
  headers `static_assert` this, and `Names` spelling is user-facing input for console commands and
  the manifest. Adding an enumerator usually means touching more than the enum: check for a
  parallel visuals/data table and any editor `switch` that maps the value.
- **`DoxygenGroups.hpp` is the only file allowed to use `@addtogroup`;** everything else uses
  `@ingroup`.
- **A `/*` inside a `/** */` block breaks the lint gate** (clang `-Wcomment`) — write glob paths
  like `shaders/*.vert` without the glob when they appear inside a doc block.
- `Texture.cpp` is the sole `STB_IMAGE_IMPLEMENTATION`; do not add a second.

## Deeper references

`docs/ARCHITECTURE.md` (systems, loop, extension points) · `docs/RENDERING.md` (coordinate spaces,
batching) · `docs/COLLISION.md` (AABB, navigation, NPC AI) · `docs/TIME_SYSTEM.md` (day/night math) ·
`docs/EDITOR.md` · `docs/PROJECT_MANIFEST.md` · `docs/BUILDING.md` · `docs/SETUP.md`
