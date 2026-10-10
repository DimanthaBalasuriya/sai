# M6 validation

Windows x64, MSVC 19.51, Slang 2026.19, NVIDIA RTX 3050 6GB Laptop GPU.
Debug uses Khronos synchronization validation; Release validation is disabled.

- Debug/Release builds and all six CPU suites passed. New tests cover feature
  registration/order/duplicates, chained HDR resource hazards, toggling/removal,
  module startup/shutdown, error propagation, tiny extents, box transforms and
  closed sphere rings, invalid radius/segments, and index validity.
- All nine new HDR/fullscreen/feature SPIR-V binaries passed spirv-val for Vulkan 1.3.
- Runtime launch tests presented 12 frames. Resize tests presented 110 frames across
  six swapchain generations in Debug and Release; Debug reported zero validation
  warnings/errors, including cleanup. GPU timers show each named feature pass.
- Debug/Release editor smoke presented 170 frames across five swapchain generations,
  with zero Debug validation messages. Coverage includes live edits, undo/redo,
  scene save/load, a second glTF import, diagnostic views, feature combinations,
  resize and minimize/restore. Each feature checkbox receives deterministic ImGui
  mouse press/release input to disable and re-enable it. Assertions verify checkbox
  state and the presence/absence of named graph passes after each change.
- CheckFeatureModules.ps1 builds Runtime, Editor and feature tests with Skybox
  excluded, postprocess excluded, Debug Draw excluded and all optional modules
  excluded. Each configuration runs CPU feature tests and editor GPU smoke. All
  four configurations passed with zero Debug validation messages. The script
  restores the configuration and executable with all modules enabled afterward.
- The live gallery preview was inspected: camera-oriented sky, PBR meshes, selection
  box/sphere, grid/axes and generated feature controls appear correctly.
  Screenshot: `../out/m6-editor.png`.
- The original instance fixture rendered 10,001 instances over 120 Release frames
  with all features enabled. Mean frame work was 6.99 ms, p95 13.61 ms in this short
  regression run (not a sustained benchmark), with zero loader/validation messages.
  Use Run.ps1 -Preset release -Instances -SmokeFrames 120 to repeat it.

Reproduce:

```powershell
powershell -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset debug -Smoke -ResizeSmoke
powershell -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset release -Smoke -ResizeSmoke
powershell -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset debug -Smoke
powershell -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset release -Smoke
powershell -ExecutionPolicy Bypass -File ./scripts/CheckFeatureModules.ps1 -Preset debug
```

The core renderer's C++ implementation/API was left unchanged. Generic RHI support,
the HDR shader variant, new extension targets and application registration are
described in ADR 0007. Other GPU vendors/platforms remain unvalidated. The sky is
procedural, debug lines are x-ray, and feature settings are session-only. No IBL,
shadows, transparent materials or asset hot reload is implied.
