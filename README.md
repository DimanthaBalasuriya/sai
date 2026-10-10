# Engine

C++20 modular 3D engine, implementing ENGINE_SPEC.md one milestone at a time.
Lighting L2–L8 and meshless editor icons are implemented. See [lighting controls and validation](docs/lighting-validation.md), [performance](docs/perf/lighting.md), and [cluster/shadow/provider decisions](docs/adr/0014-cluster-grid-and-memory.md). [L0](docs/L0-validation.md) and [L1](docs/L1-validation.md) retain the earlier checkpoint records.

## Build and test

Requires CMake 3.25+, Ninja, Git, a C++20 compiler, Python 3 for editor shader embedding, and internet access for the first
dependency fetch, plus Slang and the Khronos Vulkan validation layer. On Windows,
install Visual Studio Build Tools with the Desktop
development with C++ workload (including CMake tools and the Windows SDK).

Use an installed Vulkan SDK/Slang compiler, or prepare portable Windows x64 tools
inside this workspace (already prepared on this machine). The optional validation
build can take several minutes and also requires Python 3.10+:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/SetupSlang.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/SetupValidation.ps1
```

From PowerShell on Windows, the build helper discovers MSVC and imports its environment.
The run helper discovers workspace-local validation layers:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset debug -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset release -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset debug
```

Alternatively, in a compiler-enabled shell with CMake and Ninja on PATH:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use the `release` preset for Release. On Linux, install GLFW's X11/Wayland development
prerequisites, Slang, and validation layers. These platforms have not been validated yet.

The runtime opens a resizable 1280x720 window and renders the bundled multi-mesh
material gallery with metallic/roughness PBR materials and textures. Pass
`--gltf path/to/scene.gltf` directly to the executable to import another static scene,
or `--instances` to run the original 10,000-cube scene, which adds a 10,001st instance.
The run helper accepts `-Gltf path/to/scene.gltf` and `-Instances` too.
The scene supplies the camera, a directional light and a point light through an extracted Render World.
It closes on Escape or the window close button. It requires
Vulkan 1.3 with dynamic rendering, synchronization2, buffer device address, descriptor
indexing and graphics queue timestamps. Unsupported devices report an explicit error.
Debug builds require validation; they do not silently run without it.

`--smoke-frames 12` closes automatically after 12 event-loop iterations.
`--resize-smoke` resizes repeatedly and minimizes/restores the window, checks at least
six swapchain generations and 90 presentations, then exits. Both report GPU timing,
presentation count, named GPU pass timings, and validation warning/error count, including shutdown messages.
Smoke tests require a graphical desktop and are separate from GPU-free CTest.
For a longer fixed-size measurement, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset release -SmokeFrames 600
```

Frame-work statistics include event polling, extraction, graph recording and GPU/WSI
waits. Resize tests include recreation stalls and should not be used as steady-state
performance measurements.

Slang runs as a build step; compiled shaders are copied beside the runtime executable.
If Slang is installed elsewhere, configure with `-DENGINE_SLANGC=/path/to/slangc`.
Use the run helper with local validation, or launch the executable directly when
validation is installed through the system Vulkan SDK. The helpers only change
process environment variables, without changing system settings or execution policy.

Tracy instrumentation is enabled by default with on-demand capture; connect a
matching Tracy profiler to inspect `Runtime frame` scopes and frame markers.
Set `ENGINE_ENABLE_PROFILING=OFF` at configure time to disable instrumentation.

CPU tests exercise module lifecycle, graph dependency ordering, RAW/WAR/WAW hazards,
cycles, invalid reads/declarations, automatic barriers, transient image reuse,
stale resource handles, and graph execution with a fake command encoder. They do
not need a GPU or desktop. The graph also records CPU time per pass for future tooling.
Scene tests cover GUID identity, reflected field validation, command merging and
undo/redo, hierarchy cycles and transform propagation, camera/light extraction,
normal matrices, snapshot ownership, and adding a mesh to a 10,000-entity World.

See `docs/adr/0001-foundation.md` and `docs/adr/0002-vulkan-bootstrap.md` for lifecycle
contracts and architecture decisions. M2 resource ownership, indexed descriptors,
and transient-image reuse are described in `docs/adr/0003-resources-render-graph.md`.
Build and GPU checks are recorded in `docs/M1-validation.md` and `docs/M2-validation.md`,
including the documented NVIDIA workaround for naming the loader-owned debug messenger.
M3 decisions are in `docs/adr/0004-scene-extraction-forward.md` and verification is
in `docs/M3-validation.md`.

The editor includes dockable Hierarchy, Inspector, Asset Browser, Console, Render
Stats, Render Features and Viewport panels. Select entities in Hierarchy; edit reflected Inspector
fields or use the transform gizmo. W/E/R select move/rotate/scale while the viewport
is hovered. Right mouse orbits, wheel zooms, and F focuses the selection. Ctrl+Z/Y
undo/redo; Ctrl+S saves the scene. Asset Browser opens glTF/GLB files and saves/loads
versioned JSON scenes. GUID fields are displayed read-only. The viewport shares
the runtime forward renderer and updates after commands in the same frame.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset debug -Smoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset release -Gltf ./content/samples/material_gallery.gltf
```

Configure `-DENGINE_BUILD_EDITOR=OFF` to omit the editor target. The runtime does
not depend on Editor; RHI retains its optional ImGui backend bridge. Dependency
versions are pinned. Asset registries/cooked data and editor scene saves live beside
each executable. Scenes reference their registry's GUID assets, so keep the asset
cache with saved scenes. Imported asset updates require restarting the client;
hot reload is a later milestone. Static triangles, PNG/JPEG textures and opaque/masked
materials are supported; animation, skins, transparent blending, IBL and shadows
are not implemented. The current GPU tables allow 256 textures and 256 materials.

M4/M5 decisions and validation are recorded in `docs/adr/0005-gltf-assets.md`,
`docs/adr/0006-live-editor.md`, `docs/M4-validation.md` and `docs/M5-validation.md`.

M6 adds a procedural skybox, HDR bloom, tonemapping and debug lines/boxes/spheres.
Open the **Render Features** tab to toggle Skybox, Debug Draw, Bloom and Tonemap,
or adjust sky intensity, bloom threshold/intensity and exposure. Sky and post effects
apply in Lit view. Debug Draw overlays a grid/axes and a unit box/sphere around the
selected mesh transform using x-ray lines; the shapes do not represent imported
mesh bounds. Settings are local to the current editor session. Tonemap disabled
means HDR colors clamp at display output; the sky does not provide IBL lighting.

The extension pipeline composes the existing forward renderer without changes to
core renderer C++ files. Each optional feature module is a separate static target.
Disable `ENGINE_FEATURE_SKYBOX`, `ENGINE_FEATURE_POSTPROCESS` or
`ENGINE_FEATURE_DEBUG_DRAW` at configure time to remove a module. Runtime and editor
use the same HDR feature pipeline; all passes remain named and CPU/GPU timed.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/CheckFeatureModules.ps1 -Preset debug
```

The removal check builds/tests each excluded module and the configuration with all
optional modules excluded, then restores the full build. It requires a desktop and
the editor target enabled. M6 decisions and verification are in
`docs/adr/0007-modular-render-extensions.md` and `docs/M6-validation.md`.

F1 adds a JSON dark theme, bundled UI/monospace/icon fonts, DPI atlas rebuilds,
animated widgets, searchable hierarchy/console and undoable component reset/removal.
Open **Panels → Theme** to edit/save tokens. Debug watches `editor/ui/theme.json`;
Release uses `ui/theme.json` beside the executable. `-Theme path` selects another file.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -ThemePanel
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -UiSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -UiDpi 1.5
```

`-UiSmoke` extends the editor regression with live DPI changes, Debug theme reload
and malformed-file recovery using a separate test file, plus component reset/remove
undo. `-UiDpi` overrides automatic monitor scaling for visual checks. See
`docs/adr/0008-editor-theme.md` and `docs/F1-validation.md` for the color precision
decision, evidence and limitations.

F2 adds named, contextual actions and editable `editor/input/bindings.json`.
Hold RMB to look and fly with WASD/QE; Shift boosts speed and scroll changes it.
Alt+LMB orbits, MMB (or Alt+Shift+LMB) pans, scroll zooms toward the cursor and F
focuses selection. Q/W/E/R select tools, X changes axes and Z changes the pivot.
Ctrl snaps a gizmo; Escape or RMB cancels a drag without an undo entry.

Click to select, Shift-click to add, Ctrl-click to toggle, or drag empty space to
box-select. Ctrl+A selects all. Editing uses Ctrl+D/Delete, Ctrl+C/V, Ctrl+Z and
Ctrl+Shift+Z/Ctrl+Y, F2 to rename, H to hide/show, Ctrl+S to save and F3 to cycle
debug views. Ctrl+P toggles an isolated play preview that restores the edit scene
when stopped; simulation is not implemented. Menus show configured shortcuts.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -InputSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Bindings 'C:\path\bindings.json'
```

Debug reads source bindings; Release reads the packaged `input/bindings.json`.
Changes take effect on restart. The action model also supports validated runtime
rebinding. See `docs/adr/0009-editor-input.md` and `docs/F2-validation.md` for
decisions, checks and limits.

F4 procedural sky is implemented before F3 as requested. Open the Procedural Sky
panel for time, weather, quality, layers and debug views. Scene components support
Inspector edits, undo and save/load.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunSky.ps1 -Preset release -Smoke
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunSky.ps1 -Preset release -Benchmark -Quality 1
```

See [F4 validation](docs/F4-validation.md) for measured GPU costs, screenshots,
editor-free/module-off checks and limitations, and [ADR 0010](docs/adr/0010-procedural-sky.md)
for the approved API changes.

F3 adds dirty transforms, optional visible/static and property inheritance
components, a dynamic spatial tree, CPU culling, incremental render transfers,
virtualized hierarchy rows and batched prefab/paste/delete undo. Add EntityFlags
or PropertyInheritance in Inspector; Render Stats shows entity counters/timings.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/BenchmarkEntities.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/BenchmarkEntities.ps1 -Editor
```

All F1–F4 feature work is complete. See [feature validation](docs/FEATURES_2-validation.md)
and [entity measurements](docs/perf/entity-optimization.md) for checks, measured
before/after results and the approved fallback for the missing E1 addendum.
