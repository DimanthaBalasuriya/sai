# 0009: Contextual editor actions and cancellable tools

F2 adds two editor-only targets: `editor_input` and `editor_scene_tools`.
The input target owns chord parsing, validated binding tables, context routing,
the ImGui/GLFW boundary and camera movement. Scene tools own selection, CPU mesh
picking, clipboard operations, visibility and pending group transforms. Both
targets disappear when `ENGINE_BUILD_EDITOR=OFF`.

Feature code handles named actions. Raw keys and mouse button codes live in
`ImGuiInput.cpp`; menus queue the same actions and obtain their shortcut labels
from the binding table. Debug loads `editor/input/bindings.json`; Release loads
`input/bindings.json` beside the executable. `--bindings` overrides either path.
`Actions::Rebind` validates a complete replacement before publishing it. Invalid
JSON or conflicting chords preserve the current table. Runtime settings UI is
deferred, as permitted by F2.

Contexts consume physical inputs in priority order: TextInput, Interaction,
ViewportFlying, Hierarchy, Viewport, Global. Interaction adds cancellation and
pointer ownership during a drag. Required modifiers are preserved by wildcard
bindings, which allow additional modifiers. Text entry blocks keyboard hotkeys;
explicit menu clicks still execute. Pointer input uses viewport image hover and
panel focus rather than relying on ImGui's mouse capture flag. Native RMB capture
disables ImGui mouse processing and enables GLFW raw motion when available;
release or focus loss restores the cursor.

A gizmo drag stores pending definitions and transform deltas without changing
World or command history. The editor applies its preview to an owned RenderWorld
snapshot. Release commits one existing ReplaceSceneCommand; Escape/RMB discards
the pending values. A source/revision check rejects stale commits. Selecting both
an ancestor and its descendant transforms that branch once. Center and individual
pivots support translation, rotation and scaling; existing TRS constraints still
reject shear and non-positive scales.

The CPU geometry cache retains imported meshes needed by current and undoable
scenes, plus the renderer's built-in cube geometry. Raycasts use mesh triangles;
box selection uses overlapping projected world bounds. Gizmo hover takes priority
over picking while a gizmo is visible. Selection commits on release after the
four-pixel click/drag threshold. F3 will replace these linear scans with its
specified spatial index; F2 does not change the scene data model.

Copy/paste remaps subtree GUIDs and retains mesh/material references. Roots copied
without their original parents preserve their world placement. Paste, duplicate
and subtree delete each use a single scene replacement command. Hide/show is an
undoable editor visibility command, including descendants and lights. Since
RenderLight has no entity identity, the editor resolves hidden and previewed
lights before handing the snapshot to the renderer. Visibility is session state
and is not written into scene files.

Ctrl+P enters an isolated play **preview**: scene definitions, selection,
visibility and edit history are restored when stopping. The engine has no game
simulation or physics module yet; this action does not add one. Saving the scene
is disabled during preview to preserve the edit scene.

Inline rename opens ancestors, scrolls the input into view and retries focus until
the input is active. Enter/focus loss commits an existing SetFieldCommand; Escape
discards the text. The camera smooths fly velocity, uses logarithmic speed changes,
and focuses selected bounds over approximately 0.25 seconds.
