# F4 procedural sky

Implemented F4 before F3, as requested. The generic scene, RHI and renderer API
changes were explicitly approved on 2026-10-10; see [ADR 0010](adr/0010-procedural-sky.md).

## What is available

- S0: optional `engine_sky`, reflected scene components, ordered and replaceable
  `ISkyLayer` instances, layer toggles and an image cubemap fallback.
- S1: spherical Rayleigh/Mie/ozone atmosphere, transmittance and isotropic
  multi-scattering LUTs, per-frame sky-view LUT, and an analytic sun disc with
  limb darkening. Atmosphere LUTs rebuild only when parameters or quality change.
- S2: time-driven/manual sun and moon directions, analytic moon phases, linked
  directional lights, procedural stars, twilight fading and restrained twinkle.
- S3: reduced-resolution compute clouds, a Perlin-Worley volume, detail/curl
  variation, height/weather shaping, adaptive Beer-Lambert raymarching, powder
  lighting, temporal reprojection with history rejection and bilateral upsampling.
- S4: Clear, Partly cloudy, Overcast and Storm JSON presets; smooth transitions;
  wind, fog, precipitation and wetness state. `ApplyWeatherPreset` is the command
  based visual-script entry point. Idle manual cloud edits synchronize weather.
- S5: sampled reverse-Z scene depth, aerial froxels, amortized diffuse sky cube,
  cloud shadow map, multiple directional lights and wet material roughness.
- S6: independently registered `layers/Aurora.cpp` and `shaders/Aurora.slang`.
  It is disabled by default and requires no change to other layer files or core.

The scene stores TimeOfDay, SkyAtmosphere, CelestialBody, StarField,
VolumetricClouds and WeatherState through the ordinary serialization, reflection
and undo paths. Select **Procedural Sky** in Hierarchy to inspect them. The
**Procedural Sky** panel provides time, quality, presets, debug views, ordering
and fallback controls; Inspector also exposes layer toggles. Light links are
editable GUID selectors. Runtime rendering has no editor dependency.

## Run

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunSky.ps1 -Preset release -Smoke
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunSky.ps1 -Preset release -Benchmark -Quality 1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunSky.ps1 -Time 0 -DebugView 6
```

Quality is 0/1/2 for Low/Medium/High. Debug views 1–10 are transmittance,
multi-scattering, sky view, cloud density, cloud steps, stars, celestial discs,
aerial froxels, weather and shadow. Weather assets ship beside both executables.
`ENGINE_FEATURE_PROCEDURAL_SKY=OFF` restores the existing M6 sky module.

## Verification

- Debug and Release: **10/10 CPU suites passed**. Sky coverage includes registered
  component edits/undo/save/load, light links, day/night/phase math, weather
  interpolation and script presets, manual cloud edits, ordering/toggles,
  reverse-Z mapping and volume compatibility.
- Release with `ENGINE_BUILD_EDITOR=OFF`: **7/7 suites passed** and the full
  **240-frame sky smoke passed**, with zero Vulkan validation warnings/errors.
- Release with editor and procedural sky disabled: **6/6 suites passed** and
  **12 presented frames**, with zero Vulkan validation warnings/errors.
- Full configuration restored afterwards: **10/10 suites passed**.
- Final Debug F1 UI smoke: **230 presentations**, five swapchain generations,
  component reset/remove undo, DPI/reload checks, zero Vulkan validation messages.
- Final Debug F2 input smoke: **350 presentations**, five generations, input,
  undo/redo, play restoration, native capture and preview cancellation checks,
  zero Vulkan validation messages.
- F4 GPU smoke exercises day/night, all quality tiers, resource recreation,
  weather transitions, all ten debug views, ordering, aurora and cubemap fallback.
  Final Release run presented **240 frames**, with zero validation messages.
- The final editor capture presented **1200 frames**, with zero validation
  messages. Screenshot: [sky panel and scene](../out/f4-editor.png).

Logs are in `out/f4-*.log`. Build-script PowerShell output can wrap Tracy's normal
stderr banner as `NativeCommandError`; the build and CTest summaries above are
the actual verification results.

## GPU measurements

RTX 3050 6 GB Laptop GPU, Vulkan, Release, 1920×1080 material gallery, noon,
default coverage 0.35. Each quality run presents 300 frames and records 285
samples after initial warm-up. Values sum sky GPU passes, excluding geometry,
bloom, tonemap, presentation and editor UI.

| Quality | Cloud divisor / steps | Mean sky ms | p95 sky ms |
|---|---:|---:|---:|
| Low | 4 / 16 | 2.37 | 3.25 |
| Medium | 4 / 32 | 2.21 | 2.74 |
| High | 2 / 96 | 5.41 | 5.83 |

Medium is close to the approximately 2 ms target, but is **not a guaranteed
2 ms budget**. Laptop clock/power variation affects these short runs; Low's
measured mean is slightly higher despite its lower workload. Night stars,
denser weather, camera angle and hardware can change cost. Per-pass GPU timings
are available in Render Stats and the runtime logs. Medium uses quarter
resolution after measurements identified clouds as the main cost.

## Practical limits

This is an initial real-time implementation, not astronomical ephemerides or
a reproduction of a production cloud authoring system. Sun declination and the
29.53-day moon cycle are approximate; stars are generated rather than a catalog.
The sky environment supplies diffuse IBL, not a specular mip-chain/BRDF LUT.
Cloud shadows use a camera-centered map and simplified scattering; tile patterns
and reduced-resolution artifacts can be visible. Linked celestial lights should
be unparented directional lights. Precipitation is reflected weather state;
rain particles and a general visual scripting engine are not included.

Geometry projects relative to the camera and depth uses reverse Z, but scene
transforms remain the engine's float representation. Double-precision world
coordinates/floating origins are a separate feature. In the editor, automatic
sky scene mutations pause during an active gizmo transaction so its revision
checks and undo behavior remain valid. Layer enable/order and debug selection
are session controls; component values and weather are serialized.
