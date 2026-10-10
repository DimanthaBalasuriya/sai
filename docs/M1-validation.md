# M1 validation

Verified on Windows x64 with MSVC 19.51.36260, Ninja, Slang 2026.19, and
workspace-local Khronos validation 1.4.313.0.

Debug and Release presets build successfully and pass the GPU-free core tests.
Both compiled vertex/fragment SPIR-V modules pass `spirv-val --target-env vulkan1.3`.

Final GPU tests used NVIDIA GeForce RTX 3050 6GB Laptop GPU, driver 616.64:

| Check | Debug | Release |
| --- | --- | --- |
| 12-frame triangle smoke | 12 presentations, exit 0 | 12 presentations, exit 0 |
| Repeated resize and minimize/restore | 110 presentations, 6 swapchain generations, exit 0 | 110 presentations, 6 swapchain generations, exit 0 |
| Warning/error callbacks including shutdown | 0 | 0 |
| GPU timestamp results | Nonzero | Nonzero |

Debug explicitly enables Khronos validation and synchronization validation.
Release leaves validation disabled by default; its callback count still catches
loader/driver warnings and errors. Both tear down GPU resources, the window,
and GLFW cleanly. Earlier Release testing also passed on Intel UHD Graphics
before adding the discrete-GPU preference.

Run the same checks with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset debug -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset release -Smoke -ResizeSmoke
```

The automated resize sequence is 1280x720 -> 800x600 -> 1600x900 -> 640x480 ->
minimize/restore -> 1024x768 -> 1280x720. Named GPU work uses dynamic rendering,
two frames in flight, synchronization2 barriers, per-image presentation semaphores,
and per-frame timestamp queries. Shaders are loaded relative to the executable.

Known limitations: only Windows has been validated; Escape and closing via the
window button have not been manually tested. Naming the loader-owned debug messenger
is omitted to avoid a confirmed NVIDIA driver crash, as documented in ADR 0002.
Validation itself remains enabled in Debug and its messages are not suppressed.

M2 remains pending user confirmation: RHI resources, upload staging, a minimal render
graph, and a textured depth-tested rotating mesh.
