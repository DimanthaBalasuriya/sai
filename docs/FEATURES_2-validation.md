# Feature document completion

All four features in `ENGINE_SPEC_FEATURES_2.md` are implemented. F4 was completed
before F3 at the user's request. The missing entity addendum was replaced by the
compatible fallback explicitly approved on 2026-10-10.

| Feature | Implementation and evidence |
|---|---|
| F1 Theme | JSON theme, fonts/DPI, widgets/animation, panels; [validation](F1-validation.md) |
| F2 Input | Contextual actions, camera/selection/gizmos, commands, play preview; [validation](F2-validation.md) |
| F3 Entities | Dirty transforms, inherited values, spatial tree, culling/incremental GPU transfer, editor caches/batches; [measurements](perf/entity-optimization.md) |
| F4 Sky | Independent atmosphere/celestial/stars/cloud/weather/aerial/IBL layers and extension proof; [validation](F4-validation.md) |

F3 extends existing module reflection/serialization rather than replacing the
core entity format. Flags and inheritance are opt-in in Inspector's Add component
menu. Prefab instantiation accepts entity definitions and remaps internal GUIDs;
no unavailable prefab asset-file schema has been invented.

Release and Debug both pass 11/11 CPU suites. The reduced editor/sky-off build
passes 7/7 suites and a 12-frame, 10,001-instance runtime smoke. GPU input smoke
presents 350 frames, sky regression 240, and the live 100k hierarchy run 240;
all finish with zero Vulkan validation warnings/errors. Theme/DPI smoke also
passes. Results are recorded in `out/f3-*.log`. The Release editor
and runtime are built with procedural sky enabled by default. Module-removal
checks verify that editor and sky can be excluded independently of entity logic.

Performance limits and approximate sky-budget results remain documented in each
feature's validation file. Hardware-dependent timings are measurements, not
guarantees. Play remains the specified editor preview; general gameplay simulation
and optional per-depth job parallelization are outside these features.
