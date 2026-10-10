# M2 validation

Verified 2026-10-10 on Windows x64 using MSVC 19.51.36260, Ninja, Slang
2026.19, GLM 1.0.1, and workspace-local Khronos validation 1.4.313.0.
GPU: NVIDIA GeForce RTX 3050 6GB Laptop GPU, driver 616.64.

| Check | Debug | Release |
| --- | --- | --- |
| Build | Passed | Passed |
| CPU CTest suites | 2/2 passed | 2/2 passed |
| 12-frame textured mesh smoke | 12 presentations, exit 0 | 12 presentations, exit 0 |
| Resize and minimize/restore | 110 presentations, 6 swapchain generations, exit 0 | 110 presentations, 6 swapchain generations, exit 0 |
| Warning/error callbacks, including cleanup | 0 | 0 |
| Named GPU mesh timing, final resize run | 0.021376 ms | 0.022112 ms |

Debug requires Khronos validation and enables synchronization validation. Release
does not enable validation by default; it still checks loader/driver warning and
error callbacks. GPU timings are sample measurements, not performance benchmarks.
Present only transitions the color target and can round to zero milliseconds.

Both new mesh SPIR-V stages passed `spirv-val --target-env vulkan1.3`.
The compiler emits column-major matrices and the RHI uses SPIR-V entry point `main`.
A 360-frame Debug preview exited cleanly with zero validation warnings/errors.
Its window-only capture, `out/m2-mesh.png`, was inspected: the blue checker cube
occludes the orange checker cube, which is drawn afterward. Rotation uses elapsed
time in the runtime loop. The mesh has separate UVs on each face and uses uploaded
vertex/index buffers, sampled textures, indexed descriptors, and a D32 depth target.

GPU-free tests cover stable topological sorting, explicit dependencies, RAW/WAR/WAW
hazards, cycles, uninitialized reads, invalid IDs, duplicate uses/imports, resource
kind mismatches, first-use discard, write-to-read and same-state write barriers,
compatible non-overlapping transient reuse, rejection of incompatible/overlapping
reuse, handle generation invalidation, stable record addresses, and RAII destruction.
An uninitialized device plus fake command encoder exercises named pass boundaries,
barriers before callbacks, declared-resource access, CPU timing, repeated execution,
stale plan rejection, and callback exception conversion without GPU initialization.

Reproduce the final build and runtime checks:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset debug -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Build.ps1 -Preset release -Smoke -ResizeSmoke
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/Run.ps1 -Preset debug
```

Layering review found no Vulkan types or includes in Renderer, Runtime or tests.
Resource and graph limits are documented in ADR 0003. Only Windows/NVIDIA has been
validated for M2. The existing loader-owned debug-messenger naming exception in
ADR 0002 remains. GLM's pinned upstream CMake files emit deprecation notices with
the installed CMake; these do not affect compilation, tests or Vulkan validation.
