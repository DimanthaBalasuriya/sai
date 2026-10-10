# F2 validation

Validated on Windows with MSVC 19.51 and the RTX 3050 Laptop GPU, Vulkan 1.3.

- Debug and Release: nine CPU suites. New suites cover normalized/invalid chords,
  modifier conflicts, context priority, text capture, transactional rebinding,
  restart-loaded binding labels, focus animation and flying movement; selection,
  subtree clipboard/delete undo, transform preview/cancel/commit, stale commits,
  parent/child translation/rotation/scale, individual pivots, mesh raycasts,
  projected box bounds, surface-pick exclusions and visibility/light previews.
- Debug and Release input integration: 360 iterations, 350 presented frames each, five swapchain
  generations, zero Vulkan validation warnings/errors. Exercises tool shortcuts,
  axes/pivot toggles, duplicate/copy/paste/delete/undo/redo, hide undo, debug view,
  save, isolated play restoration, typing suppression, rename cancellation,
  typed rename commit/undo, Escape/RMB pending-transform cancellation without a
  world change or undo entry, and captured camera movement/speed/release.
- A fresh process loading `out/f2-rebound-bindings.json` passes the same integration
  checks with Move changed from W to T and debug-view cycling from F3 to F4.
- Native desktop pointer check: dragged the visible X gizmo to approximately
  x=1.44, saved the committed transform, undid it, then checked that Escape and RMB
  cancellation restored the saved baseline. Normal GLFW events were used. The
  automated captured-camera portion instead supplies deterministic mouse samples
  at the input boundary while still calling actual GLFW capture/release; keyboard
  input and text entry use the normal ImGui event queue.
- F1 regression: 240 iterations, 230 presentations, five swapchain generations,
  zero Vulkan validation messages. Live DPI/theme changes, malformed theme
  recovery, component reset/remove undo and the existing M5/M6 checks pass.
- Editor-free Debug: six CPU suites passed; runtime presented 12 frames with zero
  Vulkan validation messages. Restored `ENGINE_BUILD_EDITOR=ON` afterward.

Logs: `out/f2-debug-build.log`, `out/f2-release-build.log`,
`out/f2-input-smoke.log`, `out/f2-release-input-smoke.log`,
`out/f2-rebound-smoke.log`, `out/f2-f1-regression.log`,
`out/f2-runtime-only.log` and `out/f2-native-pointer.log`.
The client-window screenshot is `out/f2-editor.png`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -InputSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -Preset release -InputSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -Bindings 'C:\path\bindings.json'
```

## Practical limits

Picking considers triangle geometry rather than alpha-mask texture coverage.
Box selection uses projected bounds, including occluded objects. Hidden state and
the scene clipboard are editor-session state. Group operations currently capture
whole scenes through existing commands; F3 owns their performance improvements.
Transforms that introduce shear or non-positive scales are rejected. Play mode is
an isolated preview without simulation. No scene component or renderer dependency
on live World/ECS data was introduced.

F3 awaits the feature-by-feature confirmation requested in
`ENGINE_SPEC_FEATURES_2.md` and the missing `ENGINE_SPEC_ADDENDUM_ENTITIES.md`.
