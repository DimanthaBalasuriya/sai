# M4 validation

Windows x64, MSVC 19.51, Slang 2026.19, NVIDIA RTX 3050 6GB Laptop GPU.
Debug uses Khronos and synchronization validation; Release leaves validation off.

Both configurations build and pass four CPU suites. Import tests cover three mesh
primitives, four materials, two decoded textures, sRGB/linear interpretation, stable
GUIDs across reimport/catalog reopen, cooked buffer/texture round trips, command-based
instantiation, six extracted primitive instances and failure preservation.

Debug/Release gallery smoke tests present 12 frames and exit cleanly. Resize/minimize
tests present 110 frames across six swapchain generations. Debug reports zero Vulkan
warnings/errors, including shutdown. Window-only `out/m4-gallery.png` was inspected:
the split-material cube, two textured spheres and floor appear at their node transforms.
A visual inspection caught and fixed per-batch instance-address indexing.

The official Khronos MetalRoughSpheresNoTextures sample imports 123 primitives,
99 material records and 123 rendered instances. Debug's 12-frame run passes with
zero validation messages. Sample attribution and pinned source commit are retained
in `content/samples/khronos/SOURCE.txt` and `README-upstream.md`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset debug
./out/build/debug/apps/runtime/engine_runtime.exe --gltf ./content/samples/khronos/MetalRoughSpheresNoTextures.gltf --smoke-frames 12
```

Use the Run helper for workspace-local validation; when launching directly, set
VK_LAYER_PATH to `.tools/validation/bin` first. `--instances` retains the M3 fixture.
Limits and unsupported glTF features are recorded in ADR 0005. This milestone uses
the gallery and Khronos spheres rather than Sponza; no claim of complete glTF coverage
or environment-lighting fidelity is made.
