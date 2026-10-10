# 0010: Procedural sky integration

The user requests F4 before F3. This supersedes the feature order in the supplied
spec. F4's S0-S6 milestones remain the scope; F3 optimization is not included.

## Existing boundaries and gaps

The current removable render-feature framework is suitable for SkyRenderFeature
and its ordered layers. The existing HDR/tonemap chain and graph GPU timers can
also be reused. Vulkan remains private to the RHI.

Three missing capabilities prevent completing F4 through module-only additions:

1. World has six fixed component types. Component storage, reflection and scene
   decoding contain switches for those types. An independent sky module cannot
   register its six reflected/serialized components with the current API.
2. RHI exposes graphics draws, 2D sampled images and color/depth targets, but no
   compute dispatch/storage images or volume/cubemap resource descriptions.
   S3 explicitly requires compute clouds; S1/S5 need volumetric froxels and IBL.
3. The forward pass's depth and geometry-lighting inputs are private. The extension
   chain currently receives only HDR color. Aerial perspective, cloud shadows and
   ambient sky lighting need generic depth/environment interfaces. Reverse-Z also
   changes existing projection, depth clear/test and sky reconstruction contracts.

F4's general rule says "Ask before changing architecture". Its prohibition on
renderer-core changes also conflicts with adding the missing generic depth and
environment capabilities. Approval is required to resolve that conflict; sky
algorithms will remain in the sky module regardless.

## Proposed implementation

- Add optional `engine_sky` under `engine/sky`, depending on scene and the existing
  renderer/extensions contracts. Component definitions, reflection schemas,
  time/weather math and preset validation can be developed before integration
  approval, without changing World or the renderer.
- Add a generic scene component-registration/storage bridge so a module can
  provide its reflected schema, defaults and validation. Preserve existing
  components, GUID identity, command undo/redo and old scene files. Sky components
  use the same Inspector/serialization/command paths, rather than separate UI-only
  settings masquerading as scene components. The module owns sky-specific rules.
- Extend the RHI with generic compute pipelines/dispatch, sampled/storage volume
  and cube images and their barriers/descriptors. All Vulkan code stays in RHI.
  Add CPU contract tests and GPU validation for the new resource lifetime paths.
- Add generic owned depth/normal/environment handoff interfaces to the forward
  pipeline/render snapshot, plus reverse-Z support. Core code handles generic
  rendering capabilities only; atmosphere/cloud/weather algorithms and shaders
  live in `engine/sky`. This requires an explicit exception to the literal
  "no renderer-core changes" rule for those interfaces.
- SkyRenderFeature owns separate layer files. A shared SkyContext carries LUTs,
  celestial directions, weather, frame-slot history and output resources.
  Layers can be enabled/reordered/replaced without renderer edits. Keep the old
  M6 sky available until the new feature passes integration, then select one sky
  implementation at a time to avoid duplicate backgrounds.
- S0 proves ordering/toggles and an image-based SkyboxCubemap fallback. S1 builds
  atmosphere LUTs and sun/time-light integration. S2 adds moon/stars. S3 adds
  reduced-resolution, temporally reprojected compute clouds. S4 adds weather
  transitions/presets and a reflection-driven script stub. S5 integrates depth
  fog/aerial perspective, amortized ambient IBL and cloud shadows. S6 adds a
  separate aurora layer without changing existing layers.
- Measure Low/Medium/High at 1080p, report per-layer GPU timing and the measured
  total against the approximately 2 ms Medium target. Do not claim the target
  without measurement. Test an editor-free build and preserve F1/F2 regressions.

## References

Hillaire, *A Scalable and Production Ready Sky and Atmosphere Rendering Technique*
(2020): https://diglib.eg.org/items/8a3e5350-18b3-46bd-9274-3add5af88c75
and the author's paper: https://sebh.github.io/publications/egsr2020.pdf

Schneider and Vos, *Nubis: Authoring Real-Time Volumetric Cloudscapes with the
Decima Engine* (SIGGRAPH 2017), author-hosted publication links:
https://www.schneidervfx.com/

Status: the user explicitly approved the generic API additions and renderer
interface exception on 2026-10-10. S0-S6 are implemented and validated; see
[F4 validation](../F4-validation.md) for checks, measurements and practical limits.

The actual renderer handoff uses sampled depth and diffuse environment/shadow
images; a separate normal buffer was unnecessary. Forward geometry projects
relative to the camera with reverse Z. Scene modules register owned reflected
field values, keeping EnTT private. GPU descriptors and histories are owned per
fenced frame slot, with resource retirement before recording a new frame.

The procedural module replaces the M6 sky registration when enabled. The M6
module remains the build-time fallback when procedural sky is disabled; the new
module also contains its own image cubemap layer. Medium clouds use quarter
resolution and 32 steps after GPU measurement. Approximate isotropic atmosphere
multi-scattering and simplified cloud light sampling keep the initial module
small enough to inspect and extend. Aurora has a separate shader entry and layer
registration; existing layers do not depend on its implementation.
