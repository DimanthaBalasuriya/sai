# Lighting L1: Components and directional lighting

L1 is implemented. L2–L7 await the user's milestone review.

## Run and review

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Run.ps1 -Preset debug -LightingSmoke
```

In the editor, select Sun in Hierarchy (or use Procedural Sky > Sun), then rotate
its Transform in Inspector or with the rotation gizmo. The light and linked sky
update live. The default sun is 100,000 lux and the camera is EV100 15. Create >
Light adds reflected DirectionalLight, PointLight or SpotLight components. Inspector
offers linear color, optional temperature, brightness/unit selection, converted
lux/cd, enabled and stored future shadow parameters. Point/spot shading and shadow
rendering are explicitly marked unavailable in this checkpoint.
The editor reports capacity before creation at 4 directional / 4096 local lights,
counting hidden lights as well; disabled lights retain their GPU slots.

Lighting presets selects Sunny day, Overcast, Indoor or Night and changes the sun
and camera together as one undo entry. It targets the lowest-GUID directional
light and camera. Other lights/environment settings remain authored independently.
Run helpers only override EV100 when `-EV100` is passed. Scene save/load preserves
all new component settings.

Procedural Sky > Sun direction source selects Scene light transform (new default),
Time of day or Manual celestial direction. Select Time of day before using its
clock controls; `RunSky.ps1` does this automatically. Existing saved legacy sky
settings keep their selected source.

## Verification

- Debug and Release: all 13 CTest suites passed. The added suite covers physical
  conversions, half-angle spot solid angle, invalid/nonfinite values, brightness
  preservation, preset undo/redo, JSON round trips, inherited transforms/properties,
  enabled slots, camera-only edits, source/topology changes and queued uploads
  across both frame buffers. Release enforces zero heap allocations during a
  steady-state dirty extract plus CPU light-buffer bookkeeping; MSVC Debug STL
  iterator instrumentation is excluded from that allocation assertion.
- Editor-tool checks verify parent gizmo preview updates the GPU direction without
  mutating World, cancel restores the snapshot, and visibility undo restores lights.
- Sky-link checks verify rotation and physical intensity agree with the authored
  sun, including artist unit conversion and the full authoring range.
- Release build with editor and procedural sky disabled: all 9 available suites
  passed; its 120-frame lighting GPU smoke also passed with zero validation
  messages. The lighting module works without those features.
- Debug and Release lighting GPU smoke: 120 presentations each, zero validation
  messages, 2,432 bytes uploaded and 15 records packed in total. Each light edit
  uploads 64 bytes once to each frame slot; idle and camera-only frames upload zero.
  The test exercises rotation, illuminance, coupled presets and undo, enabled,
  artist unit conversion and structural point-light creation.
- Debug editor smoke: 170 presentations, 5 swapchain generations, zero validation
  messages. Includes physical sun rotation with undo/redo, save/load, feature
  toggles, debug views and resizing.
- Debug sky smoke: 240 presentations with day/night, weather, quality and layer
  changes, zero validation messages. The window was 1920x1061 on this desktop.
- Default gallery: 900 presentations, zero validation messages. Visually inspected
  `out/L1-default.png`: textured spheres and directional highlights are visible at
  EV15. The existing low-resolution sky/cloud horizon artifacts remain visible.

See [unit and upload decisions](adr/0013-light-components-gpu-units.md) and
[measurements](perf/lighting.md). GPU smokes assert CPU upload accounting and Vulkan
validation; they do not read back GPU pixels or compare golden images. Point/spot
inverse-square shading and clustering are L2; shadows are L3/L4. KHR_lights_punctual
import is not added in this checkpoint. Temperature is an approximate tint. Existing
legacy point fixtures retain their prior artist falloff until migration in L2.
