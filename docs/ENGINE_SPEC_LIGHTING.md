# Feature Spec: Lighting System

> Extends `ENGINE_SPEC.md` and `ENGINE_SPEC_ADDENDUM_ENTITIES.md`. All rules there still apply (layering, module registry, reflection, commands, extract-then-render, no Vulkan outside the RHI).
> This file replaces the lighting section (F4) of `ENGINE_SPEC_FEATURES_2.md`.
> Work one milestone at a time (L0, L1, ...). After each: summarize what was built, how to run it, known gaps and measurements, then wait for my confirmation.

---

## 1. Goals and scope

Build a **physically based, modular lighting system** that:
- renders many dynamic lights efficiently (hundreds to thousands),
- has high-quality shadows (stable, no shimmering, no acne/peter-panning),
- uses **physical units** for lights and camera exposure,
- exposes clean extension points so the procedural sky, volumetrics, post processing and later ray/path tracing plug in without core changes.

**In scope:** HDR pipeline and exposure, PBR shading, directional/point/spot lights, clustered forward lighting, shadows (cascaded + atlas), environment lighting interfaces (ambient/IBL), fog interface, editor tooling, debug views.

**Later (design for, do not build yet):** rect/disk area lights (LTC), light cookies/IES profiles, reflection probes, screen-space AO/contact shadows, volumetric lighting, ray-traced shadows, many-light sampling (ReSTIR-style) for the path tracer, baked lighting.

**Non-goals now:** deferred rendering, global illumination, lightmaps.

---

## 2. Research summary and the decisions it drives

| Topic | What the research says | Decision |
|---|---|---|
| **Light units** | Unreal, Unity (glTF), Godot and BeamNG all use physical units: lux for directional, candela (or lumens) for point/spot, nits for emissive, EV100 for exposure. Real references: clear-day sunlight is about 100,000 lux, a typical living room about 50 lux, moonlight roughly 0.1 to 1 lux (sources differ). Candela = lux x distance squared. | Use physical units everywhere. Lights store lux/candela; the camera holds EV100. glTF lights import with no rescaling. |
| **Falloff** | Physically based falloff is inverse-square. Unreal uses an attenuation radius only to clamp the contribution, not to shape it. | Inverse-square plus a smooth range window so light bounds are finite for culling. |
| **Many lights** | Forward+/clustered shading adds a compute light-culling stage before shading. It is much faster than plain forward with many lights, but slower or equal with very few, and cost depends on light radius. Clustered handles depth discontinuities better than screen tiles; typical implementations bin lights into an exponential-depth froxel grid by testing each light sphere against cluster AABBs in compute. | Clustered forward (froxel grid, exponential depth slices, compute culling). Keep a plain loop path for tiny light counts as a fallback and benchmark both. |
| **Cascaded shadows** | Cascades fight perspective aliasing. Fitting each cascade tightly in light space shimmers as the camera rotates; fitting a bounding sphere and snapping the shadow camera to whole-texel steps removes shimmer. Bias must balance acne against peter-panning. | Sphere-fit cascades + texel snapping, slope-scaled and normal-offset bias, cascade blending. |
| **Area lights** | Linearly Transformed Cosines (Heitz et al. 2016) remains the most accurate and efficient real-time method for polygonal area lights, using a precomputed matrix table by roughness and view angle. Monte Carlo sampling is too noisy or slow for raster. | Rect/disk area lights via LTC, as a later milestone behind the same light-type registry. |
| **Many-light sampling** | RIS/ReSTIR-style resampling is the active research direction for direct lighting with many lights, mainly relevant to ray/path tracing. | Do not build now. Keep the GPU light layout compatible so a path tracer can reuse it. |
| **HDR range** | A 100,000 lux sun produces very large radiance values; fp16 has a limited maximum. | Pre-exposure (see 6). |

---

## 3. Architecture

New module `engine/lighting/` (CMake target, depends on `renderer` and `scene`), registered through `IModule`.

```
engine/lighting/
  LightingModule.cpp         registers components, systems, render features, light types
  components/                DirectionalLight, PointLight, SpotLight (AreaLight later)
  render/
    LightExtract.cpp         dirty lights -> GpuLight buffer (Render World)
    ClusteredLighting.cpp    cluster build + cull + light buffers
    ShadowMaps.cpp           cascades, atlas, shadow passes
    EnvironmentLighting.cpp  provider management (IEnvironmentLighting)
    Exposure.cpp             exposure, pre-exposure, (later) histogram auto exposure
    FogProviders.cpp         ExponentialHeightFog (ISceneFogProvider)
  shaders/ (Slang)           lighting.slang, brdf.slang, shadows.slang, cluster_*.slang, exposure.slang
  debug/                     light gizmos, debug views
```

**Light type registry.** Each light type registers: the component, a packer (`component + WorldTransform -> GpuLight`), a shader type ID and the shader evaluation function. Adding a new light type later (area, IES, etc.) means one component, one packer and one shader function, with no changes to the cluster or shadow code.

**Data flow:** World (components) -> extract (dirty only) -> Render World (`GpuLight` buffer, shadow requests) -> render graph passes. Shaders never read World data.

---

## 4. Components (reflected, serialized, inspector-editable)

| Component | Fields |
|---|---|
| `DirectionalLight` | color (linear), temperature (K, optional), illuminance (lux), angular diameter (for soft shadows later), cast shadows, shadow settings (cascade count, max distance, split blend, bias, normal bias, filter quality), `enabled` |
| `PointLight` | color, temperature, intensity (value plus unit: candela / lumen / unitless), range, source radius (for specular highlights, later), cast shadows, shadow settings |
| `SpotLight` | as point plus inner and outer cone angles |
| `Camera` (extend) | exposure mode (Manual EV100 / Auto), EV100, exposure compensation, auto-exposure min/max EV, adaptation speeds |
| `EnvironmentSettings` (scene singleton) | active environment provider and its parameters, active fog provider, global ambient intensity multiplier |

Rules:
- Position and direction come from `WorldTransform`. Do not duplicate them in the light components.
- Unit conversion helpers: point `cd = lumen / (4 pi)`; spot lumen-to-candela convention must be documented in an ADR (recommended: spread the flux over the cone's solid angle). Show the converted values in the inspector.
- **Defaults:** new directional light = 100,000 lux (clear day), new camera = EV100 15 (the "sunny 16" baseline), new point light = a few hundred candela with matching exposure for indoor scenes. A "Lighting presets" menu (Sunny day, Overcast, Indoor, Night) sets sun/exposure pairs together so the scene is never accidentally black or white.
- Optional **unitless/artist mode** per light for quick prototyping; it must be clearly marked in the UI.

---

## 5. GPU data

```c
struct GpuLight {            // 64 bytes, 16-byte aligned, shared with a future path tracer
    float3 position;   float invRangeSq;       // world or camera-relative
    float3 direction;  uint  type;             // type id from the light type registry
    float3 color;      uint  shadowIndex;      // color already multiplied by intensity (physical units)
    float  spotScale;  float spotOffset;       // angle attenuation: saturate(cosAngle * scale + offset)
    uint   flags;      float _pad;             // castShadow, enabled, ...
};
```

- One structured buffer (bindless access, buffer device address), updated only for dirty lights; stable indices where possible.
- Directional lights live in a small separate array (up to 4); they are not culled.
- Light list is rebuilt/compacted on structure changes, not every frame.

**Attenuation (point/spot):**
`atten = 1 / max(d^2, minDist^2)` multiplied by a smooth window `saturate(1 - (d^2 * invRangeSq)^2)^2`, and for spots by the angle term squared. Clamp the minimum distance (about 1 cm) to avoid infinities.

---

## 6. HDR, exposure and tonemapping

- Scene color: `R16G16B16A16_SFLOAT`.
- **Pre-exposure:** shaders multiply final lit color by the current exposure before writing to the HDR target. This keeps values inside fp16 range even with a 100,000 lux sun, and lets post effects work in exposed space. Use the previous frame's exposure when auto exposure is on.
- Exposure from camera: `exposure = 1 / (1.2 * 2^EV100)`, then multiplied by compensation (2^EC).
- Tonemapper (end of the post chain): ACES approximation or Khronos PBR Neutral/AgX, selectable. Keep it behind the existing post-processing feature slot.
- **Auto exposure (milestone L7):** luminance histogram in compute, percentile-based average, clamped to min/max EV100, with separate adaptation speeds for brightening and darkening, applied in log space.
- Editor viewport shows current EV100 and a "clipping" overlay.

---

## 7. Clustered forward lighting

**Grid:** screen tiles of about 64 px (or a fixed 16 x 9 grid) by 24 to 32 depth slices, exponential in depth:

```
scale = N / ln(far / near);  bias = -scale * ln(near);
slice = floor(ln(viewZ) * scale + bias);
```

**Passes (render graph, compute):**
1. Optional depth prepass (also useful for ID buffer and later SSAO). If present, mark active clusters from depth to skip empty ones.
2. **Cluster build:** compute each cluster's view-space AABB; rebuild only when projection or screen size changes.
3. **Light culling:** test point lights (sphere) and spot lights (cone bounding sphere first, tighter cone test optional) against cluster AABBs; write per-cluster `{offset, count}` and a global index list (atomic counter). Cap per-cluster count (e.g., 256) and flag overflow in a debug counter.
4. **Shading:** the fragment shader reads its cluster, loops over its lights, and evaluates directional lights from the separate array.

**Rules:**
- Culling happens in view space using the `GpuLight` buffer; no CPU per-light loops after extract.
- A simple "all lights" loop path stays available (flag) for scenes with few lights, and both paths are benchmarked.
- Transparent objects use the same cluster data.

---

## 8. Shading model

- PBR metallic-roughness: GGX normal distribution, height-correlated Smith visibility, Schlick Fresnel, Lambert diffuse (Burley optional).
- Add multi-scattering energy compensation using the BRDF LUT (cheap, improves rough metals).
- The BRDF and light loop live in `lighting.slang` / `brdf.slang` behind a material-model interface, so new material models (clear coat, subsurface, cloth) can be added without touching light culling.
- Specular occlusion from ambient occlusion later; leave an AO input in the interface now (default 1.0).

---

## 9. Shadows

**General**
- Render feature `ShadowMaps` built on the render graph: shadow depth passes, an atlas allocator, per-light shadow slots referenced by `GpuLight.shadowIndex`.
- Use reverse-Z depth for better precision. Casters are culled per cascade/face using the spatial index (AABB tree), not by iterating all entities.
- Define `IShadowTechnique` so VSM/ray-traced shadows can replace the default later.
- **Caching:** a local light's shadow tile is re-rendered only if the light moved or any caster inside its range changed (use the dirty lists/spatial index from the entity system). Static lights over static geometry cost nothing per frame.

**Directional: cascaded shadow maps**
- 3 to 4 cascades, split by a log/uniform blend (configurable lambda), max shadow distance configurable.
- **Stable fitting:** fit each cascade to a bounding sphere of its frustum slice (rotation-invariant), then snap the cascade origin to whole shadow-texel increments in light space.
- Blend between cascades over a configurable region; depth clamp so casters behind the near plane still cast.
- Filtering: PCF with a rotated Poisson disk first; PCSS (contact-hardening) as an option later.
- Bias: slope-scaled depth bias plus normal-offset along the surface normal, both exposed per light; receiver-plane bias optional.

**Spot and point**
- Spot: one atlas tile with a perspective projection. Point: six faces in the atlas (cube layout in tiles).
- Atlas allocation by priority: tile resolution scales with the light's projected screen size and distance; budget caps total atlas memory; lights beyond the budget lose shadows gracefully (fade).

**Quality tiers** control resolution, cascade count, filter kernel and atlas size.

---

## 10. Environment lighting and fog interfaces (extension points)

```cpp
struct IEnvironmentLighting {   // ambient light source
    virtual void Declare(RenderGraph&, EnvContext&) = 0;   // update/capture; may be amortized across frames
    virtual EnvResources Resources() const = 0;            // SH L2 irradiance coefficients, prefiltered specular cubemap, BRDF LUT
};
struct ISceneFogProvider {      // fog / aerial perspective applied to scene color
    virtual void Declare(RenderGraph&, FogContext&) = 0;
};
struct IShadowTechnique;        // see section 9
```

- Default providers: `ConstantAmbient` (sky/equator/ground gradient), `HdriEnvironment` (image-based, for look-dev and tests), `ExponentialHeightFog`.
- IBL uses the split-sum approximation: prefiltered specular cubemap (mip per roughness) plus the BRDF LUT; diffuse from L2 spherical harmonics (9 coefficients).
- Providers register through the module registry; `EnvironmentSettings` selects the active one at runtime.
- **Sun occlusion slot:** a reserved optional texture/value multiplied into directional light visibility (default 1.0). It exists so a sky module can add cloud shadows later without shader changes.
- Reflection probes (box-projected, blended) are a later provider-compatible addition.

---

## 10a. Atmosphere, celestial lights and space views (research-driven requirements)

> Added after bug reports in the procedural sky: errors when rendering lights and the Earth's atmosphere in views from orbit/space. The sky itself lives in `ENGINE_SPEC_FEATURES_2.md` (F5). This section defines the lighting-side and numerical requirements it must satisfy. I have not seen the current sky code, so this section lists research findings, likely causes by symptom, and mandatory tests, not a diagnosis of specific lines.

### 10a.1 Research findings

1. **The sky-view LUT is a ground-view tool.** Hillaire (2020) renders the distant sky into a small latitude/longitude texture oriented around the camera's local up on the planet, so the horizon is always a horizontal line, with extra texel density near the horizon. From space a large part of that texture would be wasted on empty space, so the paper switches seamlessly to **simple ray marching on screen** for space views. Cost on a GTX 1080 at 720p: about 0.31 ms total for ground views versus about 0.5 ms for space views. A public reference shader even outputs nothing for the sky-view LUT when the camera is outside the atmosphere.
2. **Aerial perspective LUT range is limited.** Its default is 32 x 32 over the screen with 32 depth slices over 32 km, enough for ground views but not for orbital distances or planets seen from far away. Unreal exposes an aerial perspective start depth (km) and a view-distance scale for tuning.
3. **The planet's shadow must be in the integral.** With the sun below the horizon the Earth's shadow is visible inside the atmosphere. Models that drop a LUT dimension ignore it. Hillaire's formulation includes a planet visibility term (and atmospheric self-transmittance) toward each light.
4. **Reference Earth parameters** (Hillaire 2020 / Bruneton): ground radius 6360 km, atmosphere top 6460 km (a 100 km layer), ground albedo 0.3. Rayleigh scattering about (5.802, 13.558, 33.1) x 10^-6 per meter, Mie scattering 3.996 x 10^-6 and absorption 4.40 x 10^-6 per meter, default Mie asymmetry g = 0.8 (Cornette-Shanks or Henyey-Greenstein), ozone absorption about (0.650, 1.881, 0.085) x 10^-6 per meter as a tent function 30 km wide centered at 25 km altitude. Ozone matters for the blue sky at the horizon and at sunset. Density scale heights are the standard values of about 8 km (Rayleigh) and 1.2 km (Mie); the copy of the paper I read had these garbled, so verify against the paper.
5. **Multiple scattering has a cheap O(1) approximation.** A small 2D LUT (32 x 32 in the paper, 64 sample directions) that treats scattering of order two and above as isotropic. fp16 storage is enough. It breaks down for very dense atmospheres and strongly anisotropic phase functions. Without it the sky looks too dark and inconsistent, especially at twilight.
6. **Multiple lights are part of the model.** The formulation takes N directional lights (for example sun and moon), each with its own illuminance; the multiple-scattering transfer function is multiplied by each light's illuminance. Photometric units (luminance/illuminance) are used throughout, which matches this spec.
7. **Numerical precision is a known failure source.** A bug thread and an astro-visualization paper both report artifacts from the ray/sphere intersection when the ray origin distance is close to the radius or distances are huge. Mitigations seen: do the initial intersection in double precision (OpenSpace), use a numerically stable formulation (NVIDIA, "Precision Improvements for Ray/Sphere Intersection"), keep units consistent, and avoid working very close to zero altitude in kilometer units. Separate reports describe horizon banding from depth-buffer precision that reverse-Z fixes.
8. **Outside the atmosphere, start the ray at its top.** If the camera is outside, move the ray origin to the first atmosphere intersection, march to the second intersection (or the planet), and compare against the opaque depth buffer so geometry inside the atmosphere is handled (OpenSpace).
9. **The sun disk is not in the sky-view LUT** (low resolution and nonlinear mapping). It is composited afterwards.
10. **Volumetric shadows in the atmosphere need ray marching** (LUTs cannot represent mountain or cloud shadows on the medium); the paper suggests jittered samples with temporal reprojection (about 1.0 ms at 32 samples, 720p, GTX 1080).

### 10a.2 Likely causes by symptom

| Symptom | Likely cause | Check or fix |
|---|---|---|
| Sky or planet black, empty or garbage above roughly the atmosphere top | Sky-view LUT used while the camera is outside the atmosphere | Switch to the space ray-march mode (see 10a.3) |
| Visible pop or seam while ascending through the atmosphere | Abrupt switch between LUT and ray-march paths, or the two paths disagree in brightness | Blend over an altitude band with hysteresis; run the continuity test (10a.5) |
| Noise, banding or shimmer near the horizon or at very low altitude | Intersection cancellation, mixed units, non-reverse-Z depth | Camera-relative math, stable intersection, one unit system, reverse-Z, clamp minimum altitude |
| Blocky or striped pattern over the planet or atmosphere sphere | Single-precision ray/sphere intersection at planet scale | Double-precision CPU transforms, camera-relative floats, stable formulation |
| Sun or moon too bright, too dark or clipped; night side too bright | Light illuminance not matching what the atmosphere assumes; exposure or pre-exposure mismatch; fp16 overflow; moon not driven through the atmosphere | Single source of truth for illuminance, test exposure sweep, route moon through the same pipeline |
| Sunset sky orange but the scene light stays white | Directional light color not derived from atmospheric transmittance | Light color = illuminance x transmittance(camera to light) (10a.3) |
| Terminator wrong, glow on the night side, no Earth shadow in the atmosphere | Missing planet visibility term | Add planet ray test per sample; use the terminator test (10a.5) |
| Atmosphere glow in front of nearer geometry, or halos around objects | Missing comparison with the opaque depth buffer, or wrong composite order | March only to scene depth; composite after opaque lighting |
| Distant objects or the limb banded or too weak/strong from orbit | Aerial perspective LUT range (32 km) used for orbital distances | Ray march for far ranges; tune start depth and range |
| Wrong colors at sunset or at the limb (not blue/orange) | Missing ozone, wrong Mie absorption or phase g, wrong RGB coefficients | Compare against the reference parameters (10a.1 item 4) |
| Stars or moon visible through the planet or in daylight | No planet occlusion or transmittance on stars/moon | Ray-vs-planet test and atmospheric transmittance per body |

### 10a.3 Requirements

- **R1, two modes with a transition.** `SkyAtmosphere` has a Ground mode (transmittance + multiple-scattering + sky-view + aerial perspective LUTs) and a Space mode (per-pixel ray marching using the transmittance and multiple-scattering LUTs). Blend by camera altitude over a configurable band with hysteresis so it never flickers at the boundary. Initial band: start blending at about 50 km and finish at about 120 km altitude. These numbers are my starting guess, not from the paper; tune them by looking at the continuity test results.
- **R2, numerics.** Camera position and planet transforms in double precision on the CPU; upload camera-relative values; use one unit system inside the atmosphere shaders (kilometers recommended, documented in the shader header); numerically stable ray/sphere intersection; clamp camera altitude to a small minimum; reverse-Z depth everywhere; handle the camera underground by clamping to the ground radius.
- **R3, lights driven by the atmosphere.** Sun and moon `DirectionalLight` color = illuminance x transmittance from the camera position toward the body, sampled from the transmittance LUT. Above the atmosphere transmittance is 1. Add a planet eclipse term (is the body occluded by the planet's disc from the camera?) with a soft edge from the body's angular size. The sky's sun disk and the scene's directional light must use the same illuminance value.
- **R4, depth integration.** The ray march stops at the opaque depth. Atmosphere composites after opaque lighting and before transparents/post. If the aerial perspective LUT cannot cover the distance, use ray marching for those pixels.
- **R5, ambient from the atmosphere.** `ProceduralSkyEnvironment` (the `IEnvironmentLighting` provider) is evaluated for the current altitude and fades toward a dark space ambient. Update rate adapts with altitude and sun movement.
- **R6, exposure.** Day/night and ground/space transitions must not clip the sun disk or crush the night side; the pre-exposure and auto exposure from section 6 apply. Sun and moon differ by about five to six orders of magnitude in illuminance, so check fp16 range at every exposure setting.
- **R7, stars and bodies.** Stars and the moon use atmospheric transmittance and the planet-occlusion test; the sun disk is composited after the sky LUT and has limb darkening.
- **R8, shadows.** Cascaded shadow maps are disabled or faded when the camera is far above all shadow casters; they must not produce artifacts at orbital distances.

### 10a.4 Debug tools (required for this feature)

- A mode indicator (Ground / Blend / Space), live camera altitude, and a **forced altitude slider** to test any altitude without flying there.
- Views for the transmittance, multiple-scattering, sky-view and aerial perspective LUTs; a planet-visibility (shadow) overlay; a transmittance-to-sun heatmap.
- A **reference mode**: slow brute-force ray marching with a high sample count (single scattering plus the multiple-scattering LUT), and a side-by-side or difference view against the fast path with a per-channel RMSE readout. Hillaire validated against a path tracer with RMSE per channel; this is the same idea at engine scale.

### 10a.5 Required tests

- **Altitude sweep:** render fixed views at altitudes 0 m, 1 m, 100 m, 10 km, 100 km, 1,000 km and 40,000 km. Compare against the reference mode within a tolerance.
- **Continuity:** sweep altitude in small steps across the transition band and assert the maximum frame-to-frame luminance change stays under a threshold (no pops).
- **Sun-angle sweep:** sun zenith angles 0, 80, 90, 95, 100 and 120 degrees from the ground and from orbit, including the terminator and the Earth-shadow region.
- **Views:** looking up, at the horizon, at the day-side limb from space, at the night side with moon and stars, and with a sun/moon eclipsed by the planet.
- **Numerics:** a CPU test that compares the ray/sphere intersection in float against double for random rays at radius ratios near 1 and at planetary scale.
- **Light consistency:** the scene directional light color at sunset matches the sky's sun disk color; exposure sweep never overflows fp16.

### 10a.6 Reference costs (Hillaire 2020, GTX 1080, 720p; use as a rough guide only)

Transmittance LUT 256 x 64 (40 steps) about 0.01 ms; sky-view 200 x 100 (30 steps) about 0.05 ms; aerial perspective 32^3 (30 steps) about 0.04 ms; multiple scattering 32^2 (20 steps) about 0.07 ms; space-mode on-screen ray march about 0.33 ms.

---

## 11. Editor integration

- **Create > Light** menu: Directional, Point, Spot. Default scene contains one directional light and a camera with matching exposure.
- Gizmos (debug draw, selectable through the ID buffer): light icons, range sphere (draggable handle edits range), spot cone with angle handles, directional arrow.
- Inspector: unit dropdown (lux / candela / lumen / unitless) with live conversion, color picker plus temperature slider, shadow settings group, all reflection-driven with custom widgets from the theme work.
- Lighting presets menu (Section 4).
- All edits are commands (undo/redo), light changes update the viewport the same frame via the dirty/extract path.

---

## 12. Debug views and tools

- View modes: Lit, Lighting only, Albedo, Normals, Roughness, Metallic, AO.
- Cluster heatmap (lights per cluster), cluster overflow counter, culled-light count.
- Shadow cascade colors, shadow atlas viewer (tiles, resolution, last-render frame), shadow bias test overlay.
- Per-light "solo" and "mute" in the inspector; light bounds toggle.
- Exposure readout (EV100, histogram when auto exposure exists), clipping overlay.
- GPU timers per pass: cluster build, light cull, shadow depth (per cascade/tile), shading, exposure.

---

## 12a. GPU execution requirements

**GPU selection (RHI, pure and testable)**
- Enumerate all Vulkan physical devices and require the features this spec uses (Vulkan 1.3, dynamic rendering, synchronization2, buffer device address, descriptor indexing).
- Rank candidates: discrete GPU first, then integrated, then virtual; never select a software/CPU implementation (e.g., llvmpipe). Break ties by most device-local VRAM.
- Allow an override (`--gpu=<index|name>` and an editor setting). Log the chosen adapter (name, driver, VRAM, queue families) at startup and show it in the stats panel.
- Hybrid laptops: on Windows, export `NvOptimusEnablement` and `AmdPowerXpressRequestHighPerformance` from the executable so the high-performance GPU is preferred, and document the OS per-app GPU setting as a fallback.
- If no suitable GPU exists, fail with a clear error message listing what was found and what was missing. There is no CPU rendering fallback.
- Optional capabilities (ray queries, mesh shaders, fp16, etc.) are queried and reported but never required by the lighting system. They only gate quality tiers. Pick a default quality tier from the GPU class and VRAM, with a user override.

**All heavy lighting work runs on the GPU**
- CPU only extracts dirty light data and builds the render graph. Per-light and per-pixel work stays on the GPU: cluster build, light culling, exposure histogram, SH projection and IBL prefiltering (compute), shadow and shading passes.
- Shadow caster culling starts CPU-side using the spatial index, with a GPU-driven upgrade (compute frustum culling plus indirect draws) planned once the basics work.
- Use a single graphics queue first. Async compute for cluster build, light culling and IBL prefiltering is a later optimization, expressed through render graph dependencies.
- No GPU stalls in the frame loop: persistently mapped staging/upload rings per frame in flight, no `vkQueueWaitIdle` or fence waits mid-frame except at swapchain recreation and shutdown.
- Track GPU memory with `VK_EXT_memory_budget` where available; show usage in the stats panel; the shadow atlas, IBL and cluster buffers must respect a configurable budget and degrade gracefully.

**Acceptance:** (1) the adapter selection function has unit tests with mock device lists (discrete beats integrated, software rejected, override respected); (2) the startup log and stats panel show the selected GPU; (3) GPU timers show light culling, shadow passes and shading on the GPU timeline, with no CPU waits inside the frame; (4) on a machine with both an integrated and a discrete GPU, the discrete one is chosen by default.

---

## 13. Performance targets (initial; re-measure on the real GPU and adjust)

At 1080p on a mid-range GPU, Medium quality:
- 1,000 small point lights, light culling under about 0.3 ms.
- Directional CSM (4 cascades) plus 8 shadowed local lights: shadow passes under about 2 ms with caching on a mostly static scene.
- No per-frame heap allocation in the extract path in steady state.
- Record before/after numbers in `docs/perf/lighting.md`.

---

## 14. Tests

- Unit: unit conversions (lux/candela/lumen), exposure formula, cluster slice math (CPU reference vs GPU output), sphere-AABB and cone tests, cascade split computation, texel-snapping invariance (sub-texel camera translation must not change the snapped shadow origin offset).
- Golden-image scenes (tolerance-based): material sphere grid under each light type, cascade stability scene, 1,000-light scene, shadow caching scene.
- Validation layers clean on all lighting passes.

---

## 15. Milestones

- **L0 – HDR + exposure + PBR:** HDR target, manual EV100 with pre-exposure, tonemap, GGX PBR with one hard-coded light, material sphere grid scene. *Accept:* roughness/metallic sweep looks correct; changing EV100 by 1 doubles/halves brightness; GPU selection and adapter logging from section 12a work.
- **L1 – Light components + directional:** components with units, `GpuLight` buffer, dirty extract, directional lighting, presets, inspector. *Accept:* rotating the sun in the editor updates shading live; the default scene is correctly exposed.
- **L2 – Clustered point/spot:** cluster build, culling, shading, fallback loop path, heatmap. *Accept:* 1,000+ point lights hit the culling budget; heatmap and overflow counter work; both paths match visually.
- **L3 – Directional shadows (CSM):** stable cascades, blending, PCF, bias controls, cascade debug. *Accept:* no visible shimmering when moving or rotating the camera; no acne or detached shadows on the test scene.
- **L4 – Local shadows + atlas:** spot and point shadows, atlas allocator with priorities, shadow caching. *Accept:* several shadowed lights without seams; static lights over static geometry render no shadow passes per frame.
- **L5 – Environment lighting:** `IEnvironmentLighting`, `ConstantAmbient`, `HdriEnvironment` with SH irradiance, prefiltered specular, BRDF LUT. *Accept:* switching providers in `EnvironmentSettings` changes ambient lighting live.
- **L6 – Fog interface, sun occlusion slot, tooling polish:** `ISceneFogProvider` with `ExponentialHeightFog`, sun occlusion slot, gizmos, all debug views, perf doc. *Accept:* fog providers swap without touching the renderer; every debug view works.
- **L7 – Auto exposure:** histogram compute, adaptation, editor controls. *Accept:* moving between bright and dark areas adapts smoothly with no flicker.
- **L8 (later, optional) – Rect/disk area lights (LTC):** new light type through the registry, LTC lookup tables. *Accept:* rect light highlights look correct across roughness; no changes needed in cluster/shadow core code.

---

## 16. Rules for the agent

- Follow the layering and extract-then-render rules; no `Vk*` outside the RHI.
- Every provider, technique and light type is its own file registered via the module registry.
- Use physical units consistently; any "magic number" intensity must be justified in a comment.
- Write an ADR in `docs/adr/` for: spot lumen convention, cluster grid size, tonemapper choice, shadow filtering choice, bias strategy.
- Ask before changing architecture; otherwise choose the simplest approach that satisfies this spec.
- Never add features outside the current milestone except documented extension points.

---

## 17. References

Researched for this spec (paraphrased, not copied):
- Unreal Engine docs, "Using Physical Lighting Units" (units, inverse-square falloff, EV100)
- BeamNG docs, "Physically Based Lighting" (candela/lux relationship, reference illuminance values)
- Godot docs, "Physical light and camera units" (reference illuminance, EV100 behavior)
- glTF/Unity glTFast docs, "Physical Light Units in glTF" (lux for directional, candela for point/spot, nits for emissive; exposure-awareness)
- Vulkan forward+ and clustered renderer write-ups (light culling in compute, benchmark behavior): github.com/windydarian/vulkan-forward-plus-renderer, github.com/azer89/HelloVulkan
- NVIDIA/Microsoft "Cascaded Shadow Maps" article (perspective aliasing, PCF vs VSM) and GameDev.net threads on stable cascades (sphere fitting, texel snapping)
- Heitz, Dupuy, Hill, Neubelt, "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines" (SIGGRAPH 2016) and its talk notes: advances.realtimerendering.com/s2016/s2016_ltc_rnd.pdf; LearnOpenGL area lights guest article

- Hillaire, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique" (Eurographics Symposium on Rendering / CGF 2020): diglib.eg.org/handle/10.1111/cgf14050, plus the SIGGRAPH 2020 course slides: blog.selfshadow.com/publications/s2020-shading-course/hillaire/s2020_pbs_hillaire_slides.pdf (ground-to-space support, sky-view LUT, multiple scattering LUT, Earth coefficients, space-view switch, performance table)
- Unreal Engine docs, SkyAtmosphereComponent API (aerial perspective start depth, view distance scale, multi-scattering factor, luminance factor)
- Interactive Visualization of Atmospheric Effects for Celestial Bodies (OpenSpace), arxiv.org/abs/2010.03534 (outside-atmosphere ray setup, depth buffer comparison, double precision for large distances)
- Public Hillaire-style reference shaders (FlightGear sky-view and multiple-scattering shaders) and community threads on precision problems (three.js discourse, GameDev.net); anecdotal, used only to identify likely failure modes

Well-known background papers (cited from general knowledge; verify before relying on details):
- Bruneton and Neyret, "Precomputed Atmospheric Scattering" (2008); Bruneton, "A Qualitative and Quantitative Evaluation of 8 Clear Sky Models" (2017)
- NVIDIA, "Precision Improvements for Ray/Sphere Intersection" (Ray Tracing Gems)
- Olsson, Billeter, Assarsson, "Clustered Deferred and Forward Shading" (HPG 2012)
- Karis, "Real Shading in Unreal Engine 4" (SIGGRAPH 2013); Lagarde and de Rousiers, "Moving Frostbite to PBR" (SIGGRAPH 2014); Google Filament documentation
- Dimitrov, "Cascaded Shadow Maps" (NVIDIA, 2007); Engel, "Stable Cascaded Shadow Maps" (ShaderX6)
- Bitterli et al., "Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting" (ReSTIR, 2020)
