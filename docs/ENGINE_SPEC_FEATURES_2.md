# Feature Spec: UI Theme, Input Controls, Entity Optimization, Procedural Sky

> Extends `ENGINE_SPEC.md` and `ENGINE_SPEC_ADDENDUM_ENTITIES.md`. All rules from those files still apply (layering, module registry, reflection, commands, no Vulkan outside the RHI).
> Implement in this order: **F1 Theme → F2 Input → F3 Entity optimization → F4 Procedural sky**.
> Finish one feature, show me the result, and wait for my confirmation before starting the next.

---

## F1. Editor UI Theme and Polish (start here)

### Goals
- Replace default Dear ImGui styling with a custom dark theme built from the palette below.
- Theme is **data-driven** (a JSON file), applied by one function, hot-reloadable in debug builds.
- The UI feels smooth: consistent spacing, crisp fonts, hover transitions, no visual noise.

### F1.1 Palette (design tokens)

```css
--bg-app: #101010;      --bg-surface: #121212;   --bg-raised: #181818;
--border: #212121;      --scrollbar: #303030;
--btn: #504848;         --btn-hover: #5e5656;
--text-primary: #ffffff; --text-secondary: #d3d3d3; --text-muted: #a8a8a8; --text-dim: #797979;
--positive: #15ac63;    --info: #4fbbf9;         --negative: #ce3d53;
--warning: #d8b807;     --solana: #9945ff;
```

Derived tokens (not provided by me; define them in the theme file so they can be tuned):
- `btn-active`: `#6c6464`
- `scrollbar-hover`: `#3c3c3c`, `scrollbar-active`: `#4a4a4a`
- `border-light`: `#1a1a1a`
- `info-hover`: a lighter tint of `info` (about +12% lightness)
- Selection/hover fills use `info` with alpha (0.22 selected, 0.32 active, 0.40 docking preview)

### F1.2 Files

```
editor/ui/Theme.h/.cpp        ThemeTokens struct, LoadTheme(json), ApplyTheme(ImGuiStyle&)
editor/ui/theme.json          default theme tokens (source of truth for colors and metrics)
editor/ui/Fonts.h/.cpp        font loading, DPI scaling, icon font merge
editor/ui/Widgets.h/.cpp      custom widgets (see F1.5)
editor/ui/UiAnim.h/.cpp       tiny animation helper (see F1.6)
```

### F1.3 ImGui color mapping

| ImGuiCol | Token |
|---|---|
| Text / TextDisabled | text-primary / text-dim |
| WindowBg, ChildBg | bg-surface (ChildBg may be transparent) |
| PopupBg, MenuBarBg | bg-raised |
| TitleBg / TitleBgActive / TitleBgCollapsed | bg-app / bg-raised / bg-app |
| Border, Separator | border (BorderShadow transparent) |
| SeparatorHovered / SeparatorActive | info |
| FrameBg / FrameBgHovered / FrameBgActive | bg-app / bg-raised / border |
| ScrollbarBg | transparent; ScrollbarGrab / Hovered / Active: scrollbar / scrollbar-hover / scrollbar-active |
| CheckMark, SliderGrab | info; SliderGrabActive: info-hover |
| Button / ButtonHovered / ButtonActive | btn / btn-hover / btn-active |
| Header / HeaderHovered / HeaderActive | info @0.22 / border / info @0.32 (selected rows use `info` tint) |
| Tab, TabHovered | bg-app, border |
| Selected tab (`TabSelected` or `TabActive` depending on ImGui version) | bg-surface; selected overline (`TabSelectedOverline`): info |
| Dimmed tabs (`TabDimmed*`) | bg-app, with text-muted |
| DockingPreview / DockingEmptyBg | info @0.40 / bg-app |
| ResizeGrip* | transparent / info @0.4 / info @0.7 |
| PlotLines / PlotHistogram | info / positive |
| TableHeaderBg / TableBorderStrong / TableBorderLight | bg-raised / border / border-light |
| TableRowBg / TableRowBgAlt | transparent / white @0.02 |
| TextSelectedBg | info @0.35 |
| DragDropTarget | warning |
| NavHighlight (and NavCursor if present) | info |
| ModalWindowDimBg, NavWindowingDimBg | black @0.6 |

Check which ImGui version is vendored and use the matching enum names. Do not leave any `ImGuiCol` at the default.

### F1.4 Semantic color usage

- `info`: selection, focus, links, active tab overline, sliders, info log lines
- `positive`: success, build ok, play-mode running, "saved" states, Y axis
- `negative`: errors, destructive actions (delete), stop, X axis
- `warning`: warnings, unsaved-changes indicator, drag-drop targets
- `solana` (purple, `#9945ff`): reserved for **special entity states**: prefab instances and inherited values. (This is a suggestion; tell me if you want a different use.)
- Axis colors for vector fields: X = negative, Y = positive, Z = info.

### F1.5 Style metrics (initial values, tune in `theme.json`)

- `WindowPadding 8,8`, `FramePadding 6,4`, `ItemSpacing 8,6`, `ItemInnerSpacing 6,4`, `IndentSpacing 16`
- `WindowRounding 4`, `FrameRounding 3`, `PopupRounding 4`, `ScrollbarRounding 6`, `GrabRounding 3`, `TabRounding 3`
- `WindowBorderSize 1`, `FrameBorderSize 0`, `PopupBorderSize 1`, `ScrollbarSize 10`, `GrabMinSize 10`
- Docking: no gaps between docked panels, separator thickness about 1 to 2 px
- `WindowMenuButtonPosition` right, window title align left, `Config: ConfigDragClickToInputText = true`

### F1.6 Fonts, DPI and sRGB

- UI font: a clean sans (Inter or similar, bundled under permissive license), about 14 to 15 px. Monospace font for the console and numeric fields. Merge an icon font (e.g., Lucide or Font Awesome) for toolbar and hierarchy icons.
- Rebuild fonts when the monitor DPI scale changes; scale style metrics with DPI (`ScaleAllSizes`).
- **sRGB gotcha:** ImGui colors are authored in sRGB. If the UI renders into an sRGB swapchain/target, convert the theme colors to linear before applying (otherwise the UI looks washed out). If it renders into a UNORM target, apply them as-is. Implement this as a single flag in `ApplyTheme`, and verify visually against the hex values.

### F1.7 Custom widgets

Implement in `Widgets.cpp` and use them from all inspectors:
- `PropertyRow(label, widgetFn)`: two-column layout with fixed label column and muted label color
- `DragVec3(label, vec3&)`: compact X/Y/Z fields with colored axis labels and per-axis reset on double-click
- `ComponentHeader(name, icon, enabledToggle, contextMenu)`: collapsible component panel with a remove/reset menu
- `IconButton`, `ToggleSwitch`, `SearchField` (with placeholder in text-dim and clear button)
- Status chip (colored dot + text) for play state, validation and frame stats
- Tooltips delayed about 0.5 s, styled with `bg-raised` and `border`

### F1.8 Smoothness (UiAnim)

ImGui has no built-in transitions. Add a small helper that stores a float per `ImGuiID` and eases it toward a target:
- Hover/active color blends over about 100 ms on buttons, tree rows and tabs
- Smooth scrolling for hierarchy and console (optional)
- Fades for tooltips and popups
- Animation must not require extra frames when idle; request redraw only if an animation is in progress

Other polish: persist the dock layout (ini) and provide a default layout built with `DockBuilder`; no flicker on resize; consistent 1 px borders; Esc closes popups.

### F1.9 Hot reload and acceptance

- In debug builds, watch `theme.json` and reapply on change without restarting.
- Provide a small "Theme" debug panel: show all tokens, a color picker for each, and a "save to theme.json" button.
- **Acceptance:** (1) no default ImGui blue/grey remains anywhere (menus, popups, docking previews, tables, scrollbars, tooltips, modal dim); (2) colors match the palette when sampled from a screenshot; (3) editing `theme.json` updates the UI live; (4) hover transitions work and idle UI uses no extra redraws; (5) UI stays sharp at 100%, 125% and 150% DPI; (6) text meets readable contrast (`text-dim` is for disabled/placeholder text only).

---

## F2. Keyboard and Mouse Controls

### F2.1 Architecture

Add `editor/input/` (or `engine/input/` for the generic parts):
- **Actions:** named actions (`Camera.FlyForward`, `Tool.Move`, `Edit.Duplicate`). Feature code reacts only to actions, never to raw key codes.
- **Contexts with priority:** `Global`, `Viewport`, `ViewportFlying` (while RMB held), `Hierarchy`, `TextInput`. The highest-priority active context consumes input first.
- **Bindings:** stored in `bindings.json`; support chords (Ctrl+Shift+K), modifiers, mouse buttons and scroll. Rebindable at runtime (a settings panel can come later, but the data model must support it now).
- Menu items show their shortcut text automatically from the binding table.
- **ImGui capture rules:** block hotkeys when `WantTextInput` is true; viewport input is decided by whether the viewport panel is hovered or focused, not just `WantCaptureMouse`; while RMB-look is active, capture the mouse and ignore ImGui until release.
- **Click vs drag:** drag threshold about 4 px. Gizmo hover always wins over picking.
- **Cancel:** Esc or RMB during a gizmo drag or placement restores the previous state and creates no undo entry.

### F2.2 Default bindings

**Camera**

| Input | Action |
|---|---|
| RMB + mouse | Look (cursor hidden/captured) |
| RMB + W/A/S/D, Q/E | Fly forward/left/back/right, down/up |
| RMB + scroll | Fly speed (logarithmic) |
| Shift while flying | Speed boost |
| Alt + LMB drag | Orbit around selection or point under cursor |
| MMB drag, or Alt + Shift + LMB | Pan |
| Scroll | Zoom toward cursor |
| F | Focus selection (short animation) |

**Tools**

| Input | Action |
|---|---|
| Q / W / E / R | Select / Move / Rotate / Scale (when RMB is not held) |
| X / Z | Local-world toggle / pivot mode toggle |
| Ctrl while dragging | Snap (grid, angle, surface) |

**Selection**

| Input | Action |
|---|---|
| LMB click | Select |
| Shift + click / Ctrl + click | Add / toggle |
| LMB drag on empty space | Box select |
| Ctrl + A / Esc | Select all / deselect or cancel |
| Double-click in hierarchy | Focus entity |

**Editing**

| Input | Action |
|---|---|
| Ctrl + Z / Ctrl + Shift + Z (and Ctrl + Y) | Undo / redo |
| Ctrl + D / Delete | Duplicate / delete |
| Ctrl + C / Ctrl + V | Copy / paste |
| F2 | Rename |
| Ctrl + S | Save scene |
| Ctrl + P | Toggle play mode |
| H | Hide/show selection |
| F3 | Cycle debug view (also a dropdown) |

### F2.3 Camera feel
- Light acceleration and smoothing on fly movement, speed scaling on a log scale
- Orbit pivot is the selection center, or the picked point if nothing is selected
- Focus (`F`) animates over about 0.25 s

### F2.4 Acceptance
(1) All actions work through the action layer with no raw key codes in feature code; (2) rebinding a key in `bindings.json` takes effect on restart; (3) typing in any text field never triggers a hotkey; (4) `Q/E` fly up/down only while RMB is held, and switch tools otherwise; (5) Esc cancels any in-progress drag with no undo entry; (6) unit tests cover chord parsing, context priority and conflict detection.

---

## F3. Entity Optimization System

Builds on E1 in `ENGINE_SPEC_ADDENDUM_ENTITIES.md`. Do not change the data model; optimize and measure.

### F3.1 Transform and hierarchy update
- Dirty-driven update: collect dirty entities, bucket by `depth` (counting sort, no recursion), compute `world = parentWorld * local` level by level.
- `Static` subtrees are computed once and skipped. Moving a static entity in the editor promotes it temporarily (or marks it dirty once).
- Hot loops use flat component pools and entity handles. No GUID lookups or tree walks per frame.
- Cache inherited flags (`EffectiveVisible`, `EffectiveStatic`) and recompute only when a parent changes. Opt-in property inheritance (see addendum section 4) uses the same pass.
- Reuse scratch buffers between frames (no per-frame allocations in the update).
- Optionally parallelize each depth level with the job system once single-threaded numbers are known.

### F3.2 Spatial index
- Dynamic AABB tree with fattened bounds; update an entity only when its bounds leave the fat box.
- Used by editor raycasts, box selection and CPU culling. Batched insert/remove for prefab instantiation.

### F3.3 Extract and render handoff
- Extract copies only changed instances, materials and lights into the Render World (dirty lists, no full scans).
- Group instances by mesh + material for instanced draws.

### F3.4 Editor-side costs
- Hierarchy panel: virtualized rows, cached flattened visible list rebuilt only when `hierarchyVersion` changes, lazy expansion.
- Inspector refreshes only when the selection or component version changes.
- Prefab instantiation, delete and duplicate of large selections run as single batched commands.

### F3.5 Measurement (do this first)
- Add Tracy zones for: transform update, spatial index update, extract, hierarchy panel draw.
- Add a debug panel: entity count, dirty count per frame, hierarchy depth histogram, static/dynamic ratio, update time, extract time, instance count, draw call count.
- Benchmark scenes (generated in code): 100k entities flat; 100k entities in deep hierarchies (depth about 10); 1% moving; 0% moving; mass reparent.

### F3.6 Acceptance (initial targets, adjust after measuring)
(1) With 100k entities and 0 dirty, transform update cost is near zero (under 0.05 ms); (2) with 1% dirty, update stays under about 0.5 ms single-threaded; (3) no heap allocations per frame in the steady state (verify with a counter); (4) hierarchy panel stays at full frame rate with 100k entities; (5) before/after numbers are recorded in `docs/perf/entity-optimization.md`.

---

## F4. Procedural Sky Module (no skybox images)

### Goals
A fully **modular, procedural sky**: atmosphere, sun, moon, stars, volumetric clouds, and a weather system. No cubemap images. Each part is an independent layer that can be added, removed, reordered or replaced, and later extended (aurora, rain, lightning) without touching core renderer code.

### F4.1 Module structure

```
engine/sky/                    # separate CMake module, depends on renderer + scene
  SkyModule.cpp                # registers components, systems, render features
  components/                  # SkyAtmosphere, CelestialBody, StarField, VolumetricClouds, WeatherState, TimeOfDay
  layers/                      # ISkyLayer implementations
  shaders/ (Slang)             # LUT passes, sky view, clouds, stars, composite
  systems/                     # TimeOfDaySystem, WeatherSystem
```

Layer interface:

```cpp
struct ISkyLayer {
    virtual const char* Name() const = 0;
    virtual int Priority() const = 0;                       // composite order
    virtual void Declare(RenderGraph&, SkyContext&) = 0;    // adds passes/resources
    virtual void DebugViews(DebugViewRegistry&) {}
};
```

`SkyRenderFeature` (a regular render feature) owns an ordered list of layers and composites them. A simple **`SkyboxCubemap` layer** (image-based) must also exist as a fallback and as proof that layers are interchangeable.

### F4.2 Components (all reflected, serialized, editable in the inspector)

| Component | Key fields |
|---|---|
| `TimeOfDay` | time (0 to 24h), day length, speed, paused, optional date/latitude/longitude |
| `SkyAtmosphere` | planet radius, atmosphere height, Rayleigh and Mie coefficients/scale heights, ozone, ground albedo, sun intensity, exposure hint |
| `CelestialBody` | type (Sun/Moon), direction source (time-of-day driven or manual), angular size, color, intensity, moon phase and albedo, links to a `Light` entity |
| `StarField` | density, brightness, magnitude cutoff, twinkle, color variation, optional Milky Way intensity |
| `VolumetricClouds` | layer altitude/thickness, coverage, density, type blend, wind, detail noise scale, quality tier, shadow strength |
| `WeatherState` | cloud coverage, cloud type, precipitation, wind, fog density, wetness, plus transition target and duration |

Sun and moon directions drive the scene's directional lights (linked entities), so lighting follows the sky automatically.

### F4.3 Rendering pipeline for the sky (render graph passes)

1. **Atmosphere LUTs** (Hillaire 2020 style): transmittance LUT, multi-scattering LUT (update only when atmosphere parameters change), sky-view LUT (every frame, small), aerial-perspective froxel LUT.
2. **Sky composite:** sky-view lookup per pixel for background.
3. **Celestial bodies:** sun disc with limb darkening and moon disc with phase, drawn analytically, attenuated by transmittance.
4. **Stars:** procedural or catalog-driven star points with twinkle, faded by atmosphere transmittance and sun elevation (visible at night and twilight only).
5. **Volumetric clouds:** raymarched in compute at reduced resolution (about half or quarter) with temporal reprojection and upsampling. Density from 3D Perlin-Worley shape noise + curl-noise detail, shaped by a weather map and height gradient; lighting with Beer-Lambert + powder effect and a multiple-scattering approximation; ray-march step count adaptive by distance. Optional cloud shadow map.
6. **Aerial perspective** applied to scene geometry using the froxel LUT; clouds composite after the atmosphere and before scene fog/post.
7. **Image-based lighting update:** capture the sky into a low-res cubemap or SH coefficients (amortized across frames) so the scene gets ambient light that follows time of day and weather.

All sky output is **HDR** and goes through the normal exposure/tonemap post chain. Use reverse-Z and camera-relative coordinates where relevant; handle large view distances.

### F4.4 Systems
- `TimeOfDaySystem` (Update): advances time, computes sun/moon directions, updates linked lights, moon phase.
- `WeatherSystem` (Update): interpolates `WeatherState` toward its target over the transition duration; writes derived parameters into clouds, fog and (later) precipitation. Weather presets (clear, partly cloudy, overcast, storm) are data assets.
- Both are scriptable through reflection, so visual scripts can set time, weather and presets.

### F4.5 Quality, performance and debugging
- Quality tiers (Low/Medium/High) control cloud resolution, step counts, LUT sizes and IBL update rate.
- Per-layer GPU timers and an enable toggle for each layer in the inspector.
- Debug views: transmittance/multi-scatter/sky-view LUTs, cloud density slice, weather map, cloud step-count heatmap, star-only, sun/moon-only.
- Target (adjust after measuring): sky total under about 2 ms on a mid-range GPU at 1080p on Medium quality, with clouds reprojected.

### F4.6 Milestones

- **S0 – Layer framework:** `ISkyLayer`, `SkyRenderFeature`, components registered, `SkyboxCubemap` fallback layer. *Accept:* a layer can be toggled/reordered in the editor with no renderer-core changes.
- **S1 – Atmosphere + sun:** LUTs, sky composite, sun disc, `TimeOfDay` driving a directional light. *Accept:* a full day/night cycle with believable sunrise/sunset colors and lighting.
- **S2 – Stars and moon:** star field with twilight fade, moon with phase, moon light. *Accept:* clear night sky; moon phase changes over days.
- **S3 – Volumetric clouds (basic):** shape + detail noise, ray-march, temporal reprojection, lit by sun. *Accept:* stable clouds with no obvious flicker while moving the camera; cost within budget.
- **S4 – Weather system:** `WeatherState`, presets, transitions, wind-driven cloud motion, fog density link. *Accept:* smooth transition clear to overcast to storm, driven from the editor and from a visual-script stub.
- **S5 – Lighting integration:** aerial perspective on scene, IBL update, cloud shadows, ambient follows weather. *Accept:* objects visibly respond to time and weather.
- **S6 – Extension proof:** add one new layer (e.g., aurora or a simple rain overlay) in a separate file without editing existing layers or the renderer core.

### F4.7 Rules
- Every layer is a separate file with its own shaders and parameters; layers communicate only through `SkyContext` (shared LUTs, sun/moon directions, weather values).
- No hard-coded sun direction anywhere; all consumers read from `TimeOfDay`/`CelestialBody`.
- Parameters have sensible defaults and ranges in reflection so the inspector is usable immediately.
- Record design decisions and reference papers (Hillaire 2020 atmosphere, Schneider cloud rendering) in `docs/adr/`.

---

## General rules for all four features
- Add or extend tests for non-GPU logic (theme parsing, input chords and contexts, dirty propagation, weather interpolation, time-of-day math).
- Keep every feature behind its own module/target; the runtime must still build without the editor.
- After each feature: summarize what was built, how to run it, known gaps, and measurements (for F3 and F4).
- Ask before changing architecture; otherwise choose the simplest approach that respects the specs and write an ADR.
