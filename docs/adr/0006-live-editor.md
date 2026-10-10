# ADR 0006: Live editor, commands and serialization

Status: accepted, M5.

Editor owns its ImGui context, panel registry, input adapter, camera and per-frame
viewport targets. It borrows World, asset registry, Window and Device. Panels
register through EditorPanelRegistry; adding a panel changes the registration list.
Runtime has no dependency on Editor and can build with ENGINE_BUILD_EDITOR=OFF.

The Vulkan ImGui backend remains private to RHI. Editor sees typed engine images
and opaque UI texture tokens, never Vulkan objects. GUI resources are destroyed
before the ImGui context and Device. Replaced viewport targets retire before the
next frame, after GPU completion. Imports/loads are queued outside command recording
because staging uploads wait for completion. The UI backend assumes a stable
swapchain format; ordinary tested resize preserves that format.

UI edits and gizmo gestures execute commands against World. Drag updates merge
into one undo operation. Whole transform changes validate atomically. After UI
commands, extraction produces the owned RenderWorld used by the same ForwardPipeline
as Runtime. It renders to an sRGB off-screen image, transitions it to sampled read,
then the named Editor UI pass composites it into the swapchain. Editor orbit camera
overrides the extracted scene camera without modifying scene camera components.

Inspector and JSON serialization enumerate the same reflected component fields.
GUID references are read-only in this first Inspector. Scene files are versioned
JSON with GUID asset manifests. Parsing validates the complete replacement World
before applying one undoable ReplaceSceneCommand. Saving uses a temporary file
and atomic Windows replacement. Unknown fields/versions and hierarchy cycles fail
without replacing the current World. Keep the asset registry/cooked files with scenes.
Asset manifests retain loaded dependencies of undoable worlds, so undoing an import
and saving cannot discard the restored world's asset references.

Render Stats shows GPU timestamp timings, CPU graph pass recording timings and a
frame-time plot. CPU recording timings exclude GPU execution. Lit, Albedo, Normals,
Depth and hardware Wireframe share the forward scene data and mesh batches.
