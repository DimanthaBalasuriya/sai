# Lighting performance

L0 measurements on the available NVIDIA GeForce RTX 3050 6GB Laptop GPU (6,001 MiB
device-local memory), Windows, 2026-10-10. No pre-change capture of this newly created
fixture exists; older gallery/sky measurements are not an equivalent baseline.

| Run | Resolution | Frames | Opaque GPU ms | Tonemap GPU ms | Total GPU ms | Validation messages |
|---|---|---:|---:|---:|---:|---:|
| Debug grid, EV15 initial smoke | 1280x720 | 90 | 0.276 | 0.068 | 0.380 | 0 |
| Debug grid, EV15 screenshot run | 1280x720 | 900 | 0.342 | 0.085 | 0.928 | 0 |
| Release grid, explicit GPU 0 | 1280x720 | 300 | 0.429 | 0.109 | 0.595 | 0 |

GPU numbers above are the final reported frame, not averages; screenshot capture,
desktop composition and clock changes can affect them. The 900-frame EV15 run had
CPU/frame work mean 6.929 ms, p50 6.935 ms, p95 7.899 ms (includes frame scheduling).
The grid has 49 visible spheres/material draws and no sky, bloom or debug passes.

The 240-frame Debug sky smoke at 1920x1080 finished at 2.093 ms GPU with no validation
messages. This smoke changes settings and is not a steady-state benchmark.

The combined L2-L8 measurements below use a different fixture from L0's one-light
reference scene; they are not a before/after shading comparison.

## L1 dirty buffers

Same GPU and date. Deterministic gallery smoke at 1280x720, 120 presentations,
Lighting and Tonemap features enabled. Two directional slots (sun/moon) and one
legacy point are present initially; one authored point is created near the end.

| Run | Total bytes written | Total records packed | Opaque GPU ms, final frame | Total GPU ms, final frame | CPU/frame work mean ms | Validation messages |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2,432 | 15 | 0.096 | 0.202 | 6.985 | 0 |
| Release | 2,432 | 15 | 0.096 | 0.202 | 6.943 | 0 |

Each isolated dirty light writes 64 bytes per frame buffer (128 bytes across both);
idle frames and camera-only edits write 0. Initial/full structural updates seed
the first four directional slots and populated local slots. The initial two frame
uploads are 640 bytes; eight single-light edits are 1,024 bytes across both frames;
the point creation rebuild is 768 bytes. Total: 2,432 bytes.

GPU storage reserves 4100 * 64 bytes per frame, 524,800 bytes across two frames
(512.5 KiB). CPU mapping and pending queues retain capacity. The Release unit test
observed zero allocations for steady-state dirty extract/cache bookkeeping; it
does not claim the entire render graph or UI is allocation-free.

The readiness graph pass reported CPU means 0.031 ms Debug and 0.0066 ms Release.
This pass only declares buffer readiness: packing and mapped writes happen before
it, so its duration is not a measurement of complete upload cost. GPU frame numbers
are final-frame samples; this is a small-light correctness smoke, not a many-light
benchmark or a statistically controlled comparison with L0.

## L2-L8 combined validation

Measured 2026-10-11 on the same RTX 3050 Laptop GPU, Debug build. The requested
1920x1080 window has a 1920x1061 drawable client area on this desktop. The fixture
contains 1000 small points, legacy fill, eight shadowed local lights (seven points
and one spot), sun/moon directional slots with four sun cascades, and rect/disk lights.
It also enables HDRI, fog and automatic exposure; procedural sky and bloom are off.

| Measurement | Result |
|---|---:|
| Light culling, mean over frames 10-99 | 0.133 ms |
| Cluster build, final frame | 0.006 ms |
| Light culling, final frame | 0.103 ms |
| Opaque shading, final frame | 14.502 ms |
| Height fog, final frame | 0.101 ms |
| Exposure histogram + reduction, final frame | 0.230 ms |
| Complete GPU frame, final frame | 15.111 ms |
| Frame work mean / p95 | 14.125 / 16.696 ms |
| Validation warnings/errors, 150 frames | 0 |

Culling meets the initial 0.3 ms target on this GPU. Static shadow passes are
asserted to be zero after both frame slots warm up, until the point-light move at
frame 70, which must redraw tiles. This covers the cached shadow budget; uncached
shadow rendering has not been separately averaged against the 2 ms target. The
fixture switches providers and debug views, so complete frame timings are smoke
measurements rather than controlled steady-state benchmarks.

Forced-loop and cluster HDR readbacks at 320x180 match to the six decimal places
reported by the test, both normally and with 300 broad lights overflowing cluster
lists. The test requires normalized pixel error <= 0.003 and actual overflow.
Lower memory budgets reduce list capacity; overflowing cells use the full loop.
The default index budget is 16 MiB per frame slot, including at 4K.

Debug and Release each pass 14 tests. A Release build without editor or procedural
sky passes 10 tests. The 120-frame dirty-light regression, 170-frame editor smoke
(five swapchain generations), 240-frame sky regression, and 150-frame orbital
sweep each finish with zero Vulkan validation warnings/errors. Orbital timing is
not a performance baseline: LUT rebuilds, forced altitude and reference views vary
throughout the run.

HDRI changes currently use the existing synchronous asset upload path. It can
stall while loading a new file; cached environment evaluation and steady-state
lighting do not use diagnostic readback or immediate-submit waits. GPU image
readback is restricted to the explicit comparison test. The extraction allocation
claim remains scoped to the dirty-light bookkeeping measured by the L1 test.
