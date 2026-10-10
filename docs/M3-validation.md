# M3 validation

Verified 2026-10-10 on Windows x64, MSVC 19.51.36260, Ninja, Slang 2026.19,
EnTT 3.15.0 and GLM 1.0.1. GPU: NVIDIA RTX 3050 6GB Laptop GPU, driver 616.64.

| Check | Debug | Release |
| --- | --- | --- |
| Build and CPU suites | 3/3 passed | 3/3 passed |
| 12-frame smoke | 12 presentations, exit 0 | 12 presentations, exit 0 |
| Resize/minimize/restore | 110 presentations, 6 generations, exit 0 | 110 presentations, 6 generations, exit 0 |
| Fixed-size long run | 600 presentations, exit 0 | 600 presentations, exit 0 |
| Rendered meshes after live insertion | 10,001 | 10,001 |
| Warning/error callbacks including shutdown | 0 | 0 |

Debug explicitly enables Khronos and synchronization validation. Release does not
enable validation by default, but still counts loader/driver callback messages.
Both forward shader stages passed `spirv-val --target-env vulkan1.3`.

The fixed-size runs used 1280x720, one directional light, one point light, a static
100x100 cube grid, and a larger cube added on runtime iteration 5. The World had
10,005 entities including the root, camera and lights. The added cube appears in
the next extraction and instanced draw. GPU instance data and lighting are accessed
through buffer device addresses; frame-slot host writes are flushed and synchronized.

Frame-work statistics from sequential 600-frame runs, excluding the first ten frames:

| Measurement | Debug | Release |
| --- | --- | --- |
| Mean frame work | 6.917 ms | 6.925 ms |
| Median | 1.659 ms | 6.922 ms |
| 95th percentile | 13.327 ms | 7.381 ms |
| Last opaque pass GPU sample | 0.423 ms | 0.334 ms |

Release's mean corresponds to approximately 144 frames/second on this desktop.
These are local samples, including CPU work, frame-fence/acquire waits and FIFO
presentation pacing; they are not uncapped throughput estimates. Debug's timings
show alternating waits and validation overhead. Resize-run timing includes recreation
stalls and is not used for steady-state acceptance. No cross-device performance
claim is made.

`out/m3-scene.png` is a window-only capture inspected for the full cube grid,
surface height variation, directional/point illumination and the added large cube.
The 360-frame capture run also exited with zero Debug validation messages.

CPU tests cover the existing module/graph suites plus GUID identity, duplicate IDs,
reflected names/types/ranges, invalid initial lights, command merge, undo/redo and
redo branching, hierarchy cycle rejection, parent changes and reparenting, inherited
transforms, inverse-transpose normals under nonuniform scale, camera clipping,
camera/light extraction, independently owned snapshots, unchanged revisions,
independent World source identities and adding an entity to a 10,000-mesh snapshot.

Reproduce builds and GPU checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset debug -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset release -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset release -SmokeFrames 600
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset debug
```

Layering review found no Vulkan types above RHI and no EnTT types in public Scene,
Renderer, Runtime or test APIs. Current limits: builtin cube/material assets only,
16,384 instances, one directional and up to 16 point lights, positive scale, and
fixed core component types. Spot fields are reflected/extracted but spot shading is
deferred and explicitly rejected. There is no asset importer, scene serialization,
editor, culling, shadowing or PBR yet. Those belong to subsequent milestones.
See ADR 0004 for ownership, command and extraction contracts.
