# M5 validation

Validated on Windows, MSVC 19.51, NVIDIA RTX 3050 6GB Laptop GPU, Vulkan 1.3,
with pinned ImGui docking and ImGuizmo dependencies on 2026-10-10.

- Debug and Release builds succeeded; all five CPU suites passed in both.
- Serialization tests cover every supported component, escaped strings, GUID asset
  manifests, hierarchy round-trip, undo/redo of whole-scene replacement, atomic
  transform validation, invalid-version rejection, cycle rejection and overwrite.
- Debug and Release editor `--smoke` passed: transform edit, undo/redo, save/load,
  deferred load/import of the 123-primitive Khronos scene, undo/redo across that
  import, all five views, three resizes and minimize/restore. Each
  presented 170 frames across five swapchain generations, with zero validation
  warnings/errors (Debug includes synchronization validation and shutdown checks).
- Runtime Debug/Release smoke and resize regressions passed: 12-frame launch and
  110 presentations across six swapchain generations, zero validation messages.
- Real Windows mouse input dragged the Inspector position X from 0 to 2.1400001;
  Ctrl+S persisted that edit in JSON. Ctrl+Z followed by Ctrl+S restored X to zero.
  The captured live viewport visibly moved the selected sphere with its gizmo.
- A 1,200-frame Debug editor layout run completed with zero validation messages.
  Preview: `../out/m5-editor.png`.
- The runtime target built successfully with ENGINE_BUILD_EDITOR=OFF; normal
  editor configuration was restored afterward.

Reproduce CPU/build tests with scripts/Build.ps1, and editor GPU tests with
scripts/RunEditor.ps1 -Preset debug -Smoke (or -Preset release). GPU tests require
a graphical desktop. Linux and other GPU vendors have not been validated.

Remaining scope: shader/asset hot reload, play mode, richer asset reference editors,
skybox, post effects and debug draw belong to later work. glTF support remains the
static opaque/masked PBR subset recorded for M4.
