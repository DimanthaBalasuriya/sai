# M0 validation

Verified on Windows x64 with MSVC 19.51.36260 using the Ninja Debug and Release
presets, with Tracy instrumentation enabled.

Both presets configured and built successfully. Each passed the
`core.module_registry` CTest executable, covering lifecycle ordering, dependency
validation, name validation, failure cleanup, and automatic shutdown.

Both runtime binaries passed `--smoke-frames 12`: GLFW initialized, created a
1280x720 window, processed events, requested close, destroyed the window, terminated
GLFW, and exited successfully. Interactive resize and Escape behavior are provided
but have not been manually verified. No rendering is expected in M0.

Tracy is linked and runtime scopes/frame markers are instrumented. Connecting the
standalone Tracy profiler and inspecting a live capture has not been verified.
Linux and macOS builds have not been verified.

M1 remains pending user confirmation: Vulkan RHI bootstrap, swapchain, synchronization,
and a Slang-compiled triangle.
