# ADR 0007: Removable HDR render extensions

Status: accepted, M6.

The M3 feature hook can append passes to a fixed forward target, but it cannot
replace the geometry target or expose its private depth resources. Modifying
ForwardPipeline for every effect would undermine M6's modularity check. Instead,
engine_extensions supplies FeaturePipeline, which composes the existing forward
renderer as an HDR geometry producer and executes a registered extension graph.
No files in engine/renderer are changed for M6. Applications select this composed
pipeline; Scene, Assets and core renderer remain unaware of feature modules.

Skybox, postprocess and debugdraw each have separate public include/private source
directories and independent CMake static targets. FeatureModule implements IModule
and owns a render feature. Module startup initializes GPU resources and registers
a borrowed feature; shutdown unregisters it. FeatureRegistry orders by declared
stage, rejects duplicate names, exposes enabled state and propagates recording
errors. Builtins.cpp is the only application registration list. CMake options
remove targets, factory includes and shader build steps. The host still resolves
HDR output when all optional modules are absent.

Geometry renders linear radiance into per-frame RGBA16F images. A compile-time
Slang variant bypasses the original forward shader's built-in Reinhard mapping;
the existing LDR variant remains available. RHI gains RGBA16F, line-list topology
and attachment-load support. Its default clear alpha is zero, while opaque/masked
geometry writes alpha one. Skybox composites only uncovered pixels using this
coverage mask. This avoids accessing the forward renderer's private depth target.
It is appropriate for the current static opaque/masked scope; transparent geometry
will need a richer shared depth/coverage contract.

The ordered chain is Geometry -> Skybox -> Debug Draw -> Bloom -> Tonemap -> Output.
Bloom thresholds HDR radiance at half resolution, performs separable Gaussian blur,
then composites before exposure/filmic tonemapping. Graph-declared resources supply
barriers, compatible transient-image reuse and timing. Sky/postprocessing skip
diagnostic views so normals/albedo/depth remain meaningful. With tonemap disabled,
the required output resolve clamps linear HDR into the sRGB display target; it
does not silently apply another tone operator.

Fullscreen passes own one descriptor table per fenced frame slot. WriteTexture
updates descriptors during recording before their first bind. RHI rejects updates
after binding in that frame or from a different frame slot. Transient replacements
retire outside recording after GPU completion. ColorWrite synchronization includes
attachment reads as well as writes so Debug Draw can load the current HDR color.
HDR render images are not accepted by the existing byte-RGBA upload API.

The sky is a camera-oriented procedural environment with a bright sun; cubemap/HDRI
asset ingestion and environment lighting are future work. Debug Draw provides CPU
line/box/sphere builders, per-frame GPU geometry, grid/axes, and selection markers.
It deliberately uses x-ray lines because the composed pipeline has no access to
the forward depth target. Selection boxes are transformed unit boxes, not mesh AABBs.

The editor generates controls from feature parameter metadata. Enabled state and
parameters are session rendering policy, separate from the World; entity edits
continue to use commands. Smoke tests feed normal ImGui press/release input and
assert each checkbox changes both enabled state and recorded feature passes.
