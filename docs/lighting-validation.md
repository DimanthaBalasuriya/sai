# Lighting L2–L8

Implemented clustered point/spot lighting, stable cascaded and cached local shadows,
environment providers, height fog, histogram exposure, rect/disk LTC, and editor
icons for visible entities without meshes. Icons support selection, previews and
hidden-entity filtering. Selected lights show bounds, draggable range/cone handles,
and directional arrows. Solo/mute, provider edits and handle edits use undoable commands.

Create environment/fog/exposure settings from the Create menu, then edit the selected
entity. HDRI accepts an absolute `.hdr` path. The viewport contains all lighting,
cluster, cascade, atlas and bias views. Render Stats includes cluster/overflow counts,
shadow work, rendered exposure, histogram and GPU pass timings. Area lights use nits.

Verification commands (PowerShell with ExecutionPolicy Bypass):

```powershell
./scripts/Build.ps1 -Preset debug
./scripts/Build.ps1 -Preset release
./scripts/Run.ps1 -LightingAllSmoke
./scripts/Run.ps1 -LightingCompare
./scripts/Run.ps1 -OrbitalSmoke
./scripts/RunEditor.ps1 -Smoke
```

The mixed 1080p fixture contains 1000 small colored points plus legacy fill,
seven shadowed points and one spot, and rect/disk lights. It exercises real HDRI loading,
constant/HDRI switching, height fog, exposure adaptation, all debug views, static
shadow-cache reuse, and moved-light invalidation. The comparison run reads fp16
geometry output for forced loop versus cluster, including an overflowing fixture.
It fails on nonfinite output, normalized pixel error above 0.003, or absent overflow.

Unit tests cover reflected settings/undo, exposure limits, area units, LTC data,
atlas exhaustion, cascade splits/snapping, cluster memory degradation, icon entity
enumeration, planetary float/double intersection, and blend continuity. Existing
light/scene/serialization/editor tests continue to run.

Final validation on 2026-10-11: Debug 14/14, Release 14/14, and Release without
editor/procedural sky 10/10 tests passed. Combined lighting, loop/cluster comparison,
dirty-light regression, editor, sky and orbital GPU smokes reported zero validation
warnings/errors. The comparison includes overflowing cluster lists. These are
automated correctness checks and a visual capture, not independent golden-image
certification of every material/light combination. Rect/disk lights currently
provide unshadowed LTC illumination; shadow allocation supports directional,
point and spot lights.

The sky now blends its ground LUT path to depth-limited space marching over
50–120 km, clamps underground altitude, attenuates sun/moon/stars with atmosphere
and planet visibility, fades space ambient, and disables orbital CSM. CPU orbital
reference math uses doubles; general scene authoring transforms remain floats.
The sky panel has forced altitude, mode/altitude indicators, visibility/transmission
views and a 128-sample reference/difference view with GPU-reduced RGB RMSE.
Orbital smoke exercises the listed altitude/solar-angle sweeps and reference
readback. It is a numerical/validation smoke, not an independent path-traced golden
image certification of atmospheric accuracy.

Measured performance and run limitations are in [perf/lighting.md](perf/lighting.md).
