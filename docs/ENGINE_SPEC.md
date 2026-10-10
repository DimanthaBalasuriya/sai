# Engine Spec & Agent Brief

> Read this file fully before writing any code. It is the source of truth for scope, architecture and rules.
> Placeholder engine name: `Engine` (rename later). Assumed language: **C++20**, build: **CMake**.

## 1. Vision

A **3D-only, real-time rendering engine** with a **visual-first workflow**:

- The editor viewport **always renders the live scene in real time**. Editing never means "compile, then view".
- **No text scripting for users.** Gameplay and shaders (materials) are authored in **visual graphs** (added later). The engine must be built so those graphs can be plugged in without rewrites.
- **Modular by construction.** New features (post processing, volumes, new render pipelines, new editor tools, ray/path tracing) are added as modules/plugins, not by editing core code.
- Long-term architecture is comparable to Unity/Unreal (entities + components, prefabs, GUID assets, scriptable render pipelines). File-format compatibility is NOT a goal.

## 2. Scope

**In scope (now):** Vulkan renderer, render graph, ECS scene, glTF import, Dear ImGui editor with live viewport, undo/redo, debug views.

**Later (design for, do not build yet):** visual scripting, material graph, shadows, clustered lights, post processing stack, volumetrics, GPU-driven rendering, meshlet/cluster geometry (no-LOD large meshes), ray tracing, path tracing.

**Non-goals:** 2D, mobile, networking, audio, physics (until explicitly requested), text scripting languages, Unity/Unreal file compatibility.

## 3. Tech stack

| Area | Choice |
|---|---|
| Language / build | C++20, CMake (presets), Ninja |
| Graphics API | Vulkan 1.3 (dynamic rendering, synchronization2, buffer device address, descriptor indexing) |
| Vulkan helpers | `volk` (loader), `VMA` (memory), `vk-bootstrap` (instance/device setup) |
| Windowing/input | GLFW |
| Shaders | Slang → SPIR-V (compile step in the build; hot reload in editor later) |
| ECS | EnTT (wrapped behind engine-owned API so it can be replaced) |
| Math | GLM |
| Editor UI | Dear ImGui (docking branch) + ImGuizmo later |
| Logging / profiling | spdlog, Tracy |
| Assets | cgltf or tinygltf for import; stb_image |
| Serialization | nlohmann/json (text scenes first), binary cooked formats later |
| Dependencies | vcpkg manifest or CMake FetchContent, pinned versions |

If any choice blocks progress, propose an alternative and explain why instead of silently swapping.

## 4. Architecture principles (non-negotiable)

1. **Strict layering.** Dependencies point downward only. No upward or circular includes. Enforce through separate CMake targets.
2. **The editor is a client of the engine**, never the other way around. The runtime must build and run without the editor.
3. **The World is the single source of truth.** The editor and tools modify it only through **Commands** (undo/redo). The renderer never reads the World directly.
4. **Extract, then render.** Each frame, changed World data is extracted into a **Render World** (GPU-side scene: instances, materials, lights). The renderer only reads the Render World.
5. **Vulkan stays behind the RHI.** No `Vk*` types outside `rhi/` (and the Vulkan backend). Other modules use engine handles/descriptors.
6. **Everything is data-driven and reflected.** Components register reflection metadata (name, fields, types, ranges). Inspector UI, serialization, undo and (later) visual-script nodes are generated from reflection. Do not hand-write per-component UI or save code.
7. **Features are modules.** Render features, editor panels and importers register themselves through a registry. Adding one must not require editing core files except the registration list.
8. **No global mutable singletons** (except logging/profiling). Pass an `EngineContext`/service references explicitly.
9. **GUIDs, not paths,** are the identity of assets and entities.
10. **Make debugging first-class.** Every pass is named and GPU-timed; every Vulkan object gets a debug name; validation layers are on in debug builds.

## 5. Layers & modules

```
Editor            panels, viewport, gizmos, inspector, debug views, command history
  ↓
Scene             World (ECS), components, prefabs, serialization, reflection, commands
  ↓
Renderer          render graph, render features, pipelines, render world (GPU scene), materials
  ↓
RHI               device, queues, swapchain, buffers, images, pipelines, descriptors, sync
  ↓
Platform + Core   window, input, filesystem, jobs, memory, logging, math, profiling, events

Sidecar:  Assets (import → cook → registry → streaming), ShaderCompiler, (later) Scripting/VisualGraph
```

### 5.1 Module system

- Each module is a CMake static library with a public `include/` and private `src/`.
- Modules implement a small interface:

```cpp
class IModule {
public:
    virtual ~IModule() = default;
    virtual const char* Name() const = 0;
    virtual void OnRegister(EngineContext&) {}   // register services/components/features
    virtual void OnStartup(EngineContext&) {}
    virtual void OnShutdown(EngineContext&) {}
};
```

- `ModuleRegistry` handles ordered startup and reverse-order shutdown.
- A module declares its dependencies by name; startup order is resolved from them.

### 5.2 Renderer design

- **Render graph (frame graph):** passes declare reads/writes of resources; the graph handles ordering, barriers, transient resource aliasing, and debug names. Must support compile-once/execute-per-frame later, but a simple per-frame rebuild is fine initially.
- **Render pipeline** = an object that, given a Render World and a camera, builds a render graph. First one: `ForwardPipeline`. Later: visibility buffer, hybrid, path tracer.
- **Render features** are pluggable units that add passes/resources to a pipeline (e.g., `OpaqueGeometry`, `Skybox`, `DebugLines`, later `Shadows`, `PostProcess`, `Volumetrics`). A pipeline is an ordered list of features. Post processing is a chain of features operating on the HDR color target.
- **Render World** holds: mesh instances, materials, lights, cameras, GPU buffers; designed so an acceleration structure (TLAS) can be added later.
- Prefer **bindless/indexed resources and buffer device address** for mesh/material data to keep draw submission simple and GPU-driven rendering possible later.
- **Mesh format must support clusters/meshlets later.** Keep meshes as vertex+index buffers with an extension point for cluster data. Do not bake in assumptions that a mesh is one draw call.

### 5.3 Scene design

- Entities, components, hierarchy (parent/child with transform propagation), prefab support later.
- Core components first: `Name`, `Transform`, `Hierarchy`, `MeshRenderer`, `Camera`, `Light` (directional, point, spot).
- All edits are `ICommand` objects with `Do/Undo` and merge support (for drag edits).
- Scene serialization is text (JSON) via reflection, versioned.

### 5.4 Editor design

- Dear ImGui docking layout. The **Viewport panel displays an engine render target** (the real renderer output), not a separate editor renderer.
- Panels register through `EditorPanelRegistry`. First panels: Viewport, Hierarchy, Inspector, Asset Browser, Console/Log, Render Stats/Debug.
- Editor camera (fly/orbit) is separate from scene cameras.
- Play mode (later) = clone World into a separate play World, same renderer.

### 5.5 Debug tooling (start small, grow)

- GPU/CPU timers per pass shown in-editor, frame time graph.
- Render view selector: Lit, Albedo, Normals, Depth, Wireframe (more later: overdraw, cluster colors, BVH).
- Debug draw (lines, boxes, spheres) as a render feature.
- Render graph inspector (list of passes and resources) when feasible.

## 6. Repository layout

```
/engine
  /core          # logging, math wrappers, memory, jobs, events, profiling, file system
  /platform      # window, input (GLFW)
  /rhi           # abstraction + /rhi/vulkan backend
  /renderer      # render graph, pipelines, features, render world, materials
  /scene         # world, components, reflection, commands, serialization
  /assets        # importers, asset registry, GUIDs, cooked formats
  /shaders       # Slang sources (shared includes in /shaders/common)
/editor          # ImGui app, panels, viewport
/apps
  /runtime       # runs a scene without the editor
/third_party     # or vcpkg manifest
/docs            # architecture notes, ADRs (decision records)
/tests           # unit tests for core/scene/render graph (no GPU required where possible)
```

## 7. Milestones

Work one milestone at a time. Each ends in a runnable build. Do not start the next until acceptance criteria are met and I confirm.

**M0 – Foundation**
- CMake presets, dependency setup, module targets, `IModule` + registry, logging, Tracy, GLFW window, event loop.
- *Accept:* app builds in Debug/Release, opens a window, logs, closes cleanly.

**M1 – Vulkan RHI bootstrap**
- Instance/device/queues (validation + debug names), swapchain with resize, frames in flight, command buffers, sync, VMA.
- Clear screen, then a hard-coded triangle through the Slang → SPIR-V path.
- *Accept:* resize-safe triangle, zero validation errors, clean shutdown.

**M2 – RHI resources + simple render graph**
- Buffers, images, samplers, pipeline creation, descriptors (bindless-ready), upload via staging.
- Minimal render graph with automatic barriers and named passes; depth buffer.
- *Accept:* textured, depth-tested rotating mesh rendered via a graph pass.

**M3 – Scene + Render World**
- ECS wrapper, core components, hierarchy, reflection, extract step to Render World, camera.
- Forward pipeline renders many instances with a simple lit material (directional + point lights).
- *Accept:* a scene of 10k+ simple mesh instances renders at stable frame rate; entities added in code appear.

**M4 – glTF import + assets**
- cgltf import → engine mesh/material/texture assets with GUIDs; asset registry; load a multi-mesh glTF scene with PBR basics.
- *Accept:* a multi-polygon glTF scene (e.g., Sponza) renders correctly.

**M5 – Live editor**
- ImGui docking, viewport panel showing the real renderer output, hierarchy, inspector (reflection-driven), gizmos, command history with undo/redo, scene save/load.
- Debug view selector and pass timers.
- *Accept:* move/edit entities in the editor and see the viewport update the same frame; undo/redo works; scene round-trips to disk.

**M6 – First modular extensions (proves the architecture)**
- Add Skybox, a post-processing chain (tonemap, bloom) and a debug draw feature **as render features without modifying core renderer code**.
- *Accept:* each feature can be toggled in the editor; removing a feature's module still builds.

Later: shadows, clustered lights, volumetrics, visual-script runtime, material graph, GPU-driven rendering and meshlets, ray/path tracing.

## 8. Coding rules for the agent

- C++20, no exceptions across module boundaries; use `Result`/error codes or asserts for engine errors. RAII for all GPU/OS resources.
- Naming: `PascalCase` types/functions, `camelCase` variables, `m_` prefix for members, `k` prefix for constants.
- Keep headers light (forward declare). No `using namespace` in headers.
- Every public header gets a short comment on ownership and threading expectations.
- Write unit tests for non-GPU logic (render graph compilation, commands, reflection, serialization).
- Add an ADR in `/docs/adr/` for any major decision or deviation from this spec.
- Debug builds: validation layers on, assertions on. Fix every validation message; never silence them.
- Commit in small, buildable steps with clear messages.

## 9. How to work with me

- Before each milestone: restate the plan briefly and list files you will create or change.
- Ask when a decision affects the architecture. Otherwise pick the simplest approach that respects Section 4 and note it in an ADR.
- After each milestone: summarize what was built, how to run it, known gaps, and what is next.
- Never add features outside the current milestone "because it will be needed later," except documented extension points.
