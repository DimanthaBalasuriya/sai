# ADR 0003: Device-owned resources and a single-queue render graph

Status: Accepted for M2.

The RHI owns buffers, images, samplers, pipelines, and texture tables. Clients hold
typed handles with slot generations; stale handles fail validation. Private RAII
records release Vulkan/VMA objects, including partial construction paths. Shutdown
waits for GPU idle and releases remaining records. Explicit destruction and descriptor
updates wait for idle, and are allowed outside active frame recording. The window
outlives the device; renderer clients and transient caches are destroyed first.

The renderer declares whole-resource states through named graph passes. Compilation
preserves declaration-order resource versions, adds RAW/WAR/WAW dependency edges,
combines explicit dependencies, rejects cycles and reads of uninitialized resources,
then emits transitions in stable topological order. Import a physical resource once.
First transient use must write; a pass declares a resource once. Imported handles
carry an initial state supplied by the device. The backend uses its persistent state
when recording barriers, including dependencies on a previous cached-image use.
Acquired swapchain transitions start at color-output stage to chain the WSI wait.

Compatible transient images with non-overlapping lifetimes share a physical image.
This is image reuse, rather than separate images bound to overlapping raw VMA memory.
It supplies safe lifetime aliasing without exposing allocation details above RHI.
Each frame slot has its own image cache. Resize replacements are retired and destroyed
outside the next frame, after waiting for idle. First use discards old contents;
barriers still synchronize earlier accesses. Cached plans may execute repeatedly while
their graph declarations and imports remain valid; the demo rebuilds its graph per frame.
Compiled plans borrow the graph and are invalidated by declaration changes.

The device owns a shared descriptor layout with two partially bound arrays of 16
sampled images and samplers. Push constants select uniform indices per draw. This is
an indexed-resource foundation, not an unlimited bindless implementation. Update-after-bind,
variable table capacities, storage-buffer descriptors, and GPU-driven submission are
extension points. Vertex buffers expose a device address when requested. Indexed
draws accept subranges, so mesh data is not tied to one draw call.

Uploads use mapped, flushed staging buffers and synchronous graphics-queue copies.
No per-frame uploads occur in the demo. Slang matrices are column-major to match GLM.
Pipelines specify vertex attributes, push constants, color format and optional D32
depth testing. Pipelines targeting the swapchain rebuild if its format changes.

Every graph pass receives a debug label and GPU timestamps; CPU recording time is
also collected. Graph callbacks receive only declared resource handles. Vulkan types
and allocation details remain private to RHI. M2's MeshDemo is a test client; ECS,
Render World extraction, render pipelines and feature registration begin in later
milestones. It does not establish a renderer dependency on Scene.

Current limits: one graphics queue, whole-resource states, one mip/layer per image,
32-bit indices, triangle lists, 16 named passes, fixed descriptor capacity, synchronous
uploads and idle waits for destruction. Later asynchronous uploads/destruction,
subresource tracking, multiple queues and richer pipeline descriptors can extend
these interfaces. Present has only a transition, so its GPU duration may round to zero.
