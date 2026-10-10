# 0011: Entity optimization and the missing E1 contract

The user requests completion of the entire feature document. F1, F2 and F4 are
implemented; F3 is the remaining feature. The referenced
`ENGINE_SPEC_ADDENDUM_ENTITIES.md` is absent from the workspace and Downloads.
The current scene has no Static/Visible flags, property inheritance or prefab
asset contract. Its GUIDs, core component types and serialized entity format
will be preserved.

Work that needs no new entity contract proceeds immediately: baseline profiling,
iterative dirty transforms, handle-based adjacency and depth buckets, incremental
render extraction, a dynamic AABB tree, virtualized hierarchy, cached Inspector
values and atomic batched scene commands.

For the missing E1 semantics, the proposed compatible fallback is:

- Optional reflected `EntityFlags` and `PropertyInheritance` components stored
  through the existing module-component registration/serialization API. Existing
  scenes retain their behavior when these components are absent.
- Flags expose visible/static and whether each flag inherits from the parent.
  Hidden ancestors suppress descendants; a static ancestor makes an inheriting
  subtree static. Explicit edits still dirty static transforms once.
- Property inheritance is opt-in for the currently available render properties:
  mesh color, light color and light intensity. Local authored values remain
  untouched; cached effective values are used for extraction.
- A prefab instantiation entry point accepts a set of entity definitions, remaps
  internal GUID links, and inserts them as one batched undoable command. This
  introduces no guessed prefab asset-file format.

The user approved this compatible E1 fallback on 2026-10-10. These optional
components substitute for the unavailable contract. The eventual original
addendum may require further schema alignment.

Measurements precede optimization. Benchmarks cover 100k flat and depth-10
entities, idle frames, 1% moving leaves and mass reparenting. Allocation counters
measure warmed update/extraction paths; timings report actual results rather
than assuming the target has been reached.

Implemented: handle-based intrusive parent/child adjacency, counting-sort dirty
updates, cached effective values, module-component entity indexes, incremental
owned snapshots, persistent fat-AABB proxies, renderer CPU frustum culling and
partial writes into fenced instance buffers. Batch operations retain only edited
entities and preserve the World source identity. Hierarchy rows are clipped with
ImGuiListClipper; Inspector fields are cached by selection/component version.

`SpatialIndex` is a value-only utility in core, shared by scene tests, renderer
and editor without introducing a scene-to-renderer dependency cycle. Camera,
visible-set or mesh/material grouping changes can rebuild draw batches; ordinary
transform/color edits update existing instance slots. Structural edits and stale
snapshot consumers may take an O(N) reconciliation path. Query and update scratch
is retained; commands, loading, UI and render-graph construction are outside the
steady entity-update allocation measurement.

Flags use unsigned 0/1 values through the existing reflection format. Local
`visible=0` hides the node when `inheritVisible=0` (the default). InheritVisible
uses the parent's authored/effective visibility value, while a hidden ancestor
always suppresses descendants. Static is local-or-inherited with inheritance
enabled by default; explicit edits dirty it once. Properties inherit the nearest
parent's cached effective value, with white/1 defaults when no value is authored.
