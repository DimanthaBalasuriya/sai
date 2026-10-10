# ADR 0013: Light components, units and GPU updates

Status: implemented for lighting L1, 2026-10-10.

## Ownership and registration

`engine_lighting` owns DirectionalLight, PointLight and SpotLight schemas, physical
conversion, component packers and the Lighting render feature. Its light type table
associates component IDs, shader IDs, packers and shader evaluation names. Scene
offers a generic validator and render-light extractor registration point; Scene and
Renderer do not include lighting component headers. New light types extend the
lighting registry and shader dispatch without adding component knowledge to Scene.
Point/spot evaluation names are reserved; their shading begins in L2.

Scene caches a packed render light on light entities during dirty transform updates,
including parent transforms and inherited light color/intensity. Extract copies
owned values into RenderWorld. A single entity may have only one light component.
Generic legacy Light components and their existing artist point falloff remain
compatible. New scenes create a physical directional sun.

## Units and cone convention

Directional intensity is lux. Local intensity is candela by default, with lumen and
artist authoring choices. Components serialize the chosen unit and authored value;
the GPU receives linear color multiplied by converted lux/candela. Changing the
unit preserves brightness and forms one undo entry.

Point flux assumes uniform emission over the whole sphere: `cd = lm / (4*pi)`.
Spot flux assumes uniform emission inside the **outer cone**:
`omega = 2*pi*(1-cos(outerHalfAngle))`; `cd = lm / omega`.
The equivalent `4*pi*sin(outerHalfAngle/2)^2` avoids cancellation for narrow cones.
Inner and outer angles are half-angles in radians, shown as degrees in Inspector.
The outer cone includes its later smooth penumbra; the lumen conversion does not
renormalize energy after angle attenuation. Thus authoring lumens means nominal
cone flux, rather than the exact integrated flux after L2 attenuation.

Artist value 1 maps to 100,000 lux for directional lights and 300 cd for locals.
The UI explicitly labels this mode. Optional temperature multiplies the authored
linear color by a normalized three-band Planck tint relative to 6500 K. This is an
approximation for art direction, not a colorimetric or spectral temperature model.

## GPU storage and dirty updates

The shared C++/Slang GpuLight layout is 64 bytes and 16-byte aligned. Flags contain
enabled/cast-shadow plus a legacy-local compatibility bit. Shadow indices stay
invalid in L1. One buffer reserves slots 0–3 for directional lights; locals begin
at slot 4. Counts address these ranges independently. Limits are 4 directional and
4096 local lights; exceeding capacity returns an explicit failure.

Each fenced frame slot owns a preallocated mapped buffer. Dirty light indices are
mapped to stable GPU slots, queued separately for both frames, and coalesced when
adjacent. Multiple edits before a frame consumes its queue upload the latest value
once. Disabled lights retain their indices. Structure/source changes rebuild and
compact the list; camera edits do not repack or upload lights. Editor visibility
filters and gizmo previews may require complete rebuilds of their temporary snapshot.

Physical buffer access uses the existing buffer device address RHI. The shader
selects it through counts, avoiding a pointer null comparison that would otherwise
introduce SPIR-V Int64 capability. Upload statistics report bytes actually written
to the current frame buffer and records packed during that render.

## Exposure and sky ownership

Sunny day (100,000 lux / EV15), Overcast (20,000 / EV13), Indoor (50 / EV5), and
Night (0.3 / EV-2) atomically edit the lowest-GUID directional light and camera.
Presets reset compensation, enable the sun and select white physical intensity.
They require both entities and do not change other lights or environment settings.

New sky settings follow the authored sun transform, color, temperature, intensity
and enabled state. The sky no longer overwrites those Inspector edits in that mode.
Time-of-day and manual celestial direction remain explicit alternate sources.
`RunSky.ps1` selects time-of-day mode; regular runtime/editor startup follows the sun.
The earlier sky radiance calibration remains transitional (ADR 0012). Full IBL,
local physical shading, shadow rendering and auto exposure belong to later milestones.
