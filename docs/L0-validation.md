# Lighting L0: HDR, manual exposure, PBR

Implemented the L0 checkpoint from `ENGINE_SPEC_LIGHTING.md`. L1-L7 have not started.

## Run

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Run.ps1 -Preset debug -LightingGrid -EV100 15
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Run.ps1 -Preset debug -LightingGrid -EV100 16
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -Preset debug
```

`-LightingGrid` renders 49 engine-owned spheres under one fixed 100,000 lux white
directional light, with other render effects disabled. Roughness increases left to
right (0.045 to 1); metallic increases bottom to top (0 to 1). Rebuild the fixture
with `python scripts/PrepareLightingGrid.py`, then build to copy it to the apps.

In the editor, select Scene camera and edit `ev100` / `exposureCompensation` in the
Inspector. Changes are commands, support undo/redo, and survive save/load. Render
Stats shows EV100 and the selected GPU. Render Features > Tonemap selects ACES (0)
or PBR Neutral (1), and enables the clipping overlay. Auto exposure is not yet present.

Both launch helpers accept `-Gpu 0` or an exact GPU name; the executable equivalent is
`--gpu=0`. The editor can save its next-launch preference in Render Stats. Leave the
preference empty for automatic selection. Windows fallback: Settings > System >
Display > Graphics, add the executable, and select High performance in its options.

## Verification

- Debug and Release: all 12 CTest suites passed, including exposure formula, positive compensation,
  invalid input, reflected edits, dirty extraction, undo/redo, JSON round trip and mock
  adapter selection (ranking, memory tie-break, overrides, incompatible/software rejection).
- Hybrid laptop enumeration: NVIDIA GeForce RTX 3050 6GB Laptop GPU selected by default;
  Intel UHD Graphics was enumerated but rejected for existing formatless storage-image
  requirements. NVIDIA reports 6,001 MiB device-local memory, driver 2584739840,
  graphics/present family 0/0.
- EV100 15 and 16: 900 frames each, zero Vulkan validation warnings/errors. Captures
  `out/L0-grid-ev15.png` and `out/L0-grid-ev16.png` were visually inspected: all 49 spheres
  visible, roughness/metallic progression and darker +1 EV result. The exact 2:1 ratio
  is tested before tonemapping; display-encoded pixels do not have that ratio.
- Sky regression: 240-frame day/night, quality, weather and debug-view smoke passed
  with zero validation warnings/errors, including shutdown.

- Editor: 170 presentations and 5 swapchain generations; live transforms, undo/redo,
  save/load, debug views, feature toggles and resize smoke passed with zero validation messages.
- Windows executable exports were checked in both Release binaries: NVIDIA and AMD
  high-performance preference symbols are present.
- Extreme-exposure regression: EV100 -16 with the full sky/bloom chain, 900 frames,
  zero validation messages; the captured image is uniformly saturated as expected.
  Shader storage paths clamp additive outputs to finite fp16 range. This is a
  smoke/visual check, not a GPU floating-point readback assertion.
- Release reference grid: 300 presentations, explicit `--gpu=0`, zero validation messages.
  The unsupported `--gpu=1` path returned failure and named formatless storage images
  as the missing capability instead of falling back to another adapter.

Measurements and limitations are recorded in `perf/lighting.md`. This is visual
inspection plus deterministic CPU tests, not an automated golden-image GPU readback
suite. The existing sky calibration is transitional; see ADR 0012. Clustered lights,
shadow maps, full IBL, fog providers and auto exposure remain later checkpoints.
