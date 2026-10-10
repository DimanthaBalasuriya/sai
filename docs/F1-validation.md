# F1 validation — 2026-10-10

Windows 11, MSVC 19.51, Vulkan 1.3, NVIDIA RTX 3050 6 GB Laptop GPU.
Debug uses the existing Khronos synchronization validation layer.

## Automated checks

- Debug and Release: 7/7 CPU suites passed. The added editor suite covers theme
  roundtrip, invalid JSON/hex/version rejection without replacing the working theme,
  linear/UNORM RGB handling with unchanged alpha, DPI baseline scaling, animation
  convergence/idle redraw reporting, atlas rebuilds at 100/125/150%, and merged glyphs.
- Extended editor smoke: 240 iterations, 230 presentations, five swapchain generations,
  zero Vulkan validation warnings/errors. Includes the existing M5/M6 regression,
  live 100 → 125 → 150 → 100% font atlas rebuilds, component reset/removal and undo.
  Debug also changes a separate theme file, detects the reload, rejects malformed JSON
  while retaining working colors, and reloads a recovered file. The intentional parse
  error is logged and is not a Vulkan validation error. Release disables file watching.
- Editor-free Debug configuration: six CPU suites passed; runtime smoke presented
  12 frames, zero validation messages, GPU frame 0.415 ms. Restored ENGINE_BUILD_EDITOR=ON
  and the seven-test full build afterward. The editor-free build excludes editor_ui,
  its fonts and corrected UI shader/generator.
- Corrected UI fragment SPIR-V passed spirv-val for Vulkan 1.3.

Commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset debug
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Build.ps1 -Preset release
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -UiSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/RunEditor.ps1 -Preset release -UiSmoke
```

## Visual review

Editor client-window captures are in `out/f1-theme.png`,
`out/f1-100-editor.png`, `out/f1-125-editor.png` and `out/f1-150-editor.png`.
They exclude the surrounding desktop. Fresh atlases produce crisp text at each scale;
the scale override exercises the same rebuild path as GLFW monitor scale changes.
Physical monitor switching was not tested. High-DPI narrow vector controls stack axes
to keep full numeric values readable. The Inspector scrolls when its content exceeds
the available height. The console filter stays visible above its scrolling log region.

All 21 opaque Theme panel swatches matched their JSON hex values exactly:

| Token | Expected and sampled |
|---|---|
| bg-app | #101010 |
| bg-surface | #121212 |
| bg-raised | #181818 |
| border | #212121 |
| border-light | #1a1a1a |
| btn / hover / active | #504848 / #5e5656 / #6c6464 |
| scrollbar / hover / active | #303030 / #3c3c3c / #4a4a4a |
| text-primary / secondary / muted / dim | #ffffff / #d3d3d3 / #a8a8a8 / #797979 |
| positive / info / info-hover | #15ac63 / #4fbbf9 / #8bd3fb |
| negative / warning / solana | #ce3d53 / #d8b807 / #9945ff |

Actual panel, input, menu and ordinary button fills independently sampled
#121212, #101010, #181818 and #504848. Antialiased edges and translucent selection
fills blend with their background; they are not expected to sample as opaque tokens.

## Practical limits

- The scene model has no per-component enabled flag. ComponentHeader supports an
  optional toggle, but reflected inspectors omit it; Render Features use real toggles.
- Purple is reserved for prefab/inheritance states pending the missing entity addendum.
- Component reset/remove uses the existing full-scene replacement command. Its
  large-scene cost is deferred to F3, rather than changing the entity model during F1.
- Animation schedules no extra frames and settles to NeedsRedraw=false. The live
  engine still renders continuously; this is not a whole-engine idle-render policy.
- Style CPU conversion exists as specified, but this Vulkan backend performs conversion
  in the fragment shader to avoid 8-bit near-black quantization. See ADR 0008.
- Packaged default theme files are refreshed from source during a build. Use --theme
  for a custom file that should remain independent of packaging.

F2, F3 and F4 have not started. The supplied spec requests review and confirmation
after each feature; F3 also depends on ENGINE_SPEC_ADDENDUM_ENTITIES.md.
