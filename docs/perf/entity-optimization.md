# F3 entity optimization measurements

Measured 2026-10-10, Windows, MSVC Release, Tracy enabled/on demand, Intel
i7-12650H and RTX 3050 6GB Laptop GPU. These are measured samples on this machine,
not portable performance guarantees. The original E1 addendum was unavailable;
the user approved the compatible optional-component contract in
[ADR 0011](../adr/0011-entity-optimization.md).

## Before / after

`tests/EntityBenchmark.cpp` generates 100,000 mesh entities in a flat scene and
10-node chains. It warms extraction, edits 1,000 leaves, times transform update
and owned-snapshot extraction separately, and counts C++ allocations on the
benchmark thread. Commands/setup are outside those measured intervals. Five
moving samples are averaged; idle and mass-reparent rows are single samples.
Baseline was recorded before replacing the recursive/full-scan implementation.

| Case | Transform before (ms) | Transform after (ms) | Extract before (ms) | Extract after (ms) | Allocations before → after |
|---|---:|---:|---:|---:|---:|
| Flat, idle | 0.0013 | 0.0018 | 1.3723 | 0.0003 | 0 → 0 |
| Depth 10, idle | 0.0016 | 0.0017 | 1.2292 | 0.0003 | 0 → 0 |
| Flat, 1% moving | 30.1471 | 0.0552 | 26.5598 | 0.0559 | 100,042 → 0 |
| Depth 10, 1% moving | 53.9177 | 0.0973 | 30.9175 | 0.0727 | 100,042 → 0 |
| Flat, mass reparent | 32.6433 | 1.5147 | 34.5598 | 1.5188 | 100,042 → 0 |
| Depth 10, mass reparent | 85.3784 | 14.9024 | 33.5109 | 7.8141 | 100,042 → 0 |

Mass reparent moves 9,999 roots. In the deep scene their descendants make 99,990
entities dirty. Command time is separate: flat 3.299 → 3.380 ms; depth 10
3.869 → 6.427 ms. Maintaining adjacency/depth during an edit moves a small amount
of work into the command and removes the full per-frame traversal.

The initial transform targets (<0.05 ms idle, about <0.5 ms at 1% moving) are met.
All measured update/extraction samples, including the first moving sample and
mass reparent, allocate zero after initial snapshot setup. Aligned and ordinary
C++ new/new[] are counted. This does not claim that the entire editor, sky or
render graph is allocation-free; structural edits, scene loading and stale/new
snapshot consumers are allowed to allocate.

## Spatial index

A 100,000-box grid uses a balanced dynamic tree with a 0.1-unit fat margin.
Queries retain scratch and preallocated output storage. The randomized regression
compares queries against brute-force overlap and checks removals and ray slabs.

| Operation | Measured ms | Allocations | Result |
|---|---:|---:|---|
| Move 1,000 boxes inside fat bounds | 0.0177 | 0 | 0 reinserts |
| Move 1,000 boxes outside fat bounds | 0.4901 | 0 | 1,000 reinserts |
| Ray query | 0.0036 | 0 | 1 candidate |
| Box query | 0.0512 | 0 | 1,225 candidates |
| Frustum query | 0.0343 | 0 | 1,225 candidates |

Editor picking retains triangle narrow-phase tests. Rectangle selection uses a
cropped frustum broad phase and projected bounds. The forward renderer culls
against the camera frustum before grouping mesh/material draws. Structural edits
reconcile identities and batch insert/remove proxies while retaining existing
tree nodes; regular dirty updates use cached proxy handles.

## Editor

The live `--entity-benchmark` fixture has exactly 100,000 entities, about 1,000
meshes, the gallery and procedural sky. The hierarchy is focused and displays
the flat list through ImGuiListClipper. The first 15 frames are excluded.

- Hierarchy panel: 225 samples, mean **0.0355 ms**, p95 **0.0485 ms**.
- Whole frame: 224 samples, mean **6.9252 ms**, p95 **7.3889 ms**.
- 240 presented frames, zero Vulkan validation warnings/errors.

This measures hierarchy scaling without making 100,000 visible mesh draws the
GPU bottleneck. The CPU transform/extraction benchmark separately contains
100,000 mesh instances. The old hierarchy had nested full-entity scans per row;
no fabricated timing is reported for that quadratic draw path.

Hierarchy topology/name changes rebuild adjacency; expansion/search rebuild the
cached visible rows only when they change. Transform edits preserve that cache.
Inspector values refresh by selection and component version. Large prefab/paste,
duplicate and delete edits keep one undo entry and only retain affected entities.

## Running and evidence

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/BenchmarkEntities.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/BenchmarkEntities.ps1 -Editor
```

Raw results: `out/f3-baseline.log`, `out/f3-after-final.log`,
`out/f3-editor-100k-final.log`. Earlier after-runs are retained too; laptop clock
and cache variation explain differences between runs. CPU regressions cover inherited values without changing
authored fields, serialization, stale snapshots, restored previews, static edits,
cycles/depth limits, atomic invalid batches, internal GUID remapping, undo/source
identity, spatial rotations/removal and cached 100k hierarchy rows.

Tracy zones include `Scene.TransformUpdate`, `Scene.Extract`, spatial
insert/update/query/cull, renderer spatial/handoff and hierarchy flatten/draw.
Render Stats exposes dirty/updated counts, depth histogram, static/dynamic counts,
update/extract times, total/visible instances and draw calls. Optional job-system
parallelization is unnecessary for the measured single-thread targets.
