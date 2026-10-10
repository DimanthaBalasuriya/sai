# 0008: Data-driven editor UI

F1 adds `editor_ui`, a separate editor-only static target. JSON owns all palette
tokens and spacing, font and animation metrics. Loading validates a complete
replacement before publishing it; invalid hot reloads preserve the working theme.
Debug watches the source file; Release uses the packaged file. `--theme` selects
an explicit file. Theme panel saves replace the completed temporary file atomically.

The atlas bundles Inter, Roboto Mono and Font Awesome Free, including license texts,
immutable upstream URLs and SHA-256 hashes in `editor/ui/fonts/manifest.json`.
See [Inter](https://github.com/rsms/inter),
[Roboto Mono](https://github.com/googlefonts/RobotoMono) and
[Font Awesome's font license](https://fontawesome.com/license/free).
Monitor content scale is polled through the existing GLFW editor integration.
Fonts are rasterized again at the new size, rather than stretching a bitmap.
Each style update starts from the unscaled JSON metrics. GPU font retirement waits
for idle outside command recording, before rebuilding the CPU atlas.

`ApplyTheme(style, tokens, srgbTarget, dpi)` supports the requested CPU sRGB-to-linear
flag and preserves alpha. However, ImGui's vertex color packing is 8-bit UNORM:
CPU conversion of #121212 rounds the linear channel to 2/255, which the swapchain
encodes as #161616. To satisfy screenshot palette matching, the Vulkan UI backend
instead receives authored sRGB vertices and decodes their RGB in the fragment shader.
The editor therefore calls ApplyTheme with CPU conversion disabled. Sampled
viewport images are already linear and are multiplied by the decoded vertex color;
their own sampling and attachment conversions remain correct. UNORM targets retain
the stock shader. This is a precision correction to F1.6, within the existing RHI
bridge; Vulkan stays private and the renderer/scene layering is unchanged.

ImGui 1.91.9b lacks an initialization hook for this shader, so a build generator
adapts the pinned private backend copy and embeds the Slang SPIR-V. It checks exact
replacement sites and fails if the vendor interface changes. Fetched sources remain
untouched. The generator and corrected shader are excluded with ENGINE_BUILD_EDITOR=OFF.
The combined sampler follows [Slang's SPIR-V documentation](https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/a2-01-spirv-target-specific.html).

Reusable widgets preserve ImGui activation/deactivation metadata through property
tables so command merging still works. Component reset/removal uses the existing
ReplaceSceneCommand; required components cannot be removed. The scene has no
component enabled flag, so inspector headers omit that optional control; render
feature switches use real feature enablement. Purple remains reserved for future
prefab/inherited states rather than inventing those states before the entity addendum.

UiAnim belongs to EditorApp, follows each item ID, expires old entries, and reports
NeedsRedraw only during transitions. Buttons, hierarchy/component rows and dock tabs
ease over the configured duration. Popups/tooltips fade their alpha. The live engine
continues rendering its scene; these animations do not schedule additional frames
or own timers. An idle-render policy for the entire engine is outside F1.

Escape is reserved for ImGui navigation/popups in the editor. The runtime retains
its existing Escape-to-close behavior. Dock layout persists through the existing ini
file and DockBuilder fallback. F2's action routing is deferred until feature review.
