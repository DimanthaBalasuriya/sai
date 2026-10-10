# ADR 0004: Reflected World, command edits, and extracted instanced rendering

Status: Accepted for M3.

Scene is a static library above Renderer. Its World owns an EnTT registry behind a
private implementation; no EnTT types appear in public headers. Entities use explicit
GUIDs and never expose ECS slots. SceneModule owns the runtime World and participates
in ModuleRegistry lifetime. Core supplies value math/identity types; GLM remains private.
EnTT 3.15.0 is pinned to commit d4014c74dc3793aba95ae354d6e23a026c2796db.

Name, Transform, Hierarchy, MeshRenderer, Camera and Light register field metadata:
names, types, offsets and numeric ranges. ReadField and SetFieldCommand use this
metadata, with no field-specific inspector or serialization logic. Components are
fixed engine-owned types in this milestone; dynamic reflected component registration
and serialization are later extensions. GUID asset references currently resolve to
builtin cube/material identifiers; the asset registry belongs to M4.

World mutation is private and accessible to commands. CreateEntityCommand,
SetFieldCommand and ReparentCommand support history, undo/redo and merge of repeated
field edits. Undo of creation rejects removing a parent with surviving children;
normal reverse history removes children first. Reparent preserves local transforms,
rejects cycles and nonexistent parents, and marks inherited transforms dirty.
Transforms use T*Rz*Ry*Rx*S, radians, and positive nonzero scale. World transform caches
avoid recomputing unchanged matrices and propagate parent changes to descendants.

Extraction builds an owned RenderWorld snapshot when World revision changes. It
contains mesh/material GUIDs, models, inverse-transpose normal matrices, colors,
cameras and lights. Unchanged calls reuse cached extraction data, copying into the
client snapshot. Multiple cameras select the lowest GUID deterministically. A World
has an independent random source identity so equal revision numbers from different
Worlds cannot reuse stale GPU data. These source IDs are session identities; entities
keep the explicitly supplied persistent GUIDs.

ForwardPipeline reads only RenderWorld. It holds an ordered render feature list;
OpaqueGeometry adds the initial graph pass. Additional features implement AddPasses
without an engine dependency on Scene. The initial builtin mesh group uses one
indexed instanced draw, device-address instance data, and per-frame camera/light
data. Frame-slot buffers are written only after BeginFrame waits for that slot's
fence. Flushes and HostWrite-to-StorageRead barriers precede shader accesses. Static
instance data is uploaded only when source/revision changes for that slot. Camera
projection is rebuilt for the current output aspect ratio.

Forward shading supports one directional light and up to 16 point lights, Lambert
diffuse, ambient light, checker texture, instance colors, depth testing and a simple
output compression curve. Spot parameters are reflected/extracted; spot shading is
explicitly rejected in M3. The instance capacity is 16,384; unknown asset GUIDs are
reported instead of silently replaced. Multiple meshes/material batches, PBR,
asset loading, culling, shadows and clustered lighting remain later work.

The runtime fixture creates a 100x100 cube grid, a camera, directional and point
lights through commands. An extra cube is created after startup and must appear in
the next extraction/draw. The original M2 MeshDemo remains a separate test client.
No World reads occur inside the renderer, and no Vulkan types occur above RHI.
