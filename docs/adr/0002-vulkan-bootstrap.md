# ADR 0002: Vulkan bootstrap behind RHI

Status: Accepted for M1.

The new static `engine_rhi` target depends on Core and privately on Platform and
the pinned Vulkan helper libraries. Public RHI headers expose engine-owned
results, paths, and statistics. Vulkan types and calls remain in `rhi/src/vulkan`.
The application owns a Device via RAII and borrows its Window; both die before
Platform shutdown. M1's `DrawFrame` is a bootstrap operation, replaced by general
resource and command interfaces in M2 rather than expanding this triangle API.

GLFW surface creation is performed inside the Vulkan backend using a borrowed
opaque native handle. This permits the required Vulkan surface call without
exposing Vulkan in Platform or the application. RHI privately consumes GLFW;
the current bridge is explicitly GLFW-specific, and other window backends can
provide their own private surface adapters later.

Request Vulkan 1.3 dynamic rendering, synchronization2, buffer device address and
the descriptor indexing features needed for future indexed resource tables.
Shader draw parameters are enabled because Slang's HLSL SV_VertexID lowering uses
BaseVertex. SPIR-V entry names are `main`, matching Slang's default emission. VMA
is initialized with device-address support, with allocations deferred to M2.
vk-bootstrap selects the physical device, graphics/present queues, and swapchain;
discrete GPUs are preferred with compatible integrated GPUs retained as fallback.
volk loads the Vulkan entry points. Debug builds require Khronos validation and
enable synchronization validation. Warning/error messages are counted and cause
the smoke test to fail, including messages emitted during shutdown.

Two frames in flight each own an acquire semaphore, command pool/buffer, fence,
and timestamp query pool. A presentation wait semaphore belongs to each swapchain
image rather than each CPU frame; reacquisition ensures presentation consumed the
previous wait before the semaphore is reused. Frame fences guard command reuse.
Synchronization2 barriers transition acquired images into color attachment layout
and then presentation layout. There is no render pass/framebuffer abstraction in
this dynamic-rendering bootstrap.

Framebuffer extent changes and out-of-date/suboptimal results trigger recreation.
Zero-size framebuffers defer rendering. The runtime waits for events while
minimized. Recreation waits for device idle, creates the replacement with the old
swapchain handle, then releases the old image views, presentation semaphores, and
pipeline. A format-compatible triangle pipeline is rebuilt per generation for
simplicity in M1; pipeline caching belongs to later milestones.

Slang compiles separate vertex/fragment SPIR-V modules through build dependencies.
Runtime shaders live beside the executable, so launching does not depend on the
working directory. A runtime asset target copies changed shader outputs even
when only a shader changed and the executable does not relink. Shader hot reload
is deferred. Bootstrap commands have a debug label and GPU timestamp pair;
device-owned Vulkan objects, the instance, physical device, and surface have debug
names. One documented driver workaround: naming the loader-owned debug messenger
through vkSetDebugUtilsObjectNameEXT caused an access violation inside NVIDIA
616.64's nvoglv64.dll (confirmed by a symbolized stack trace). Its name is omitted;
the messenger remains enabled and all validation messages continue to be reported.

The acquire image barrier uses COLOR_ATTACHMENT_OUTPUT for both source and
destination stages, matching the acquire semaphore wait. A NONE source stage does
not chain the layout transition after acquisition and produced a synchronization
validation write-after-read hazard during testing.

Portable Windows Slang is pinned by version and SHA-256. When the SDK is absent,
an optional script builds Khronos validation 1.4.313.0 in `.tools`, using its
upstream known-good dependencies. The run helper adds the local manifest directory
through VK_LAYER_PATH only in the child process environment, restoring its previous
value afterward. This selects the explicit manifests without a system registry
installation and avoids the loader warning from absent explicit-layer registry keys.
No system SDK or driver installation is required or modified by these helpers.
