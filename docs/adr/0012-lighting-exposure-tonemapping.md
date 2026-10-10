# ADR 0012: L0 exposure, PBR and adapter selection

L0 reuses the existing R16G16B16A16_SFLOAT forward target and registered post-processing
feature slot. Camera EV100 defaults to 15; exposure compensation defaults to zero.
Reflection, command history, serialization and incremental extraction carry both
fields. The editor's navigation camera preserves the scene camera's exposure.

The renderer camera contract owns `ManualExposure`: `exp2(EC - EV100) / 1.2`.
The unused camera-position GPU `w` component carries exposure. Lighting accumulates
in fp32, applies exposure exactly once, and clamps to the finite fp16 range before
storage. Tonemapping consumes exposed color without an additional exposure control.
Bloom therefore also works in exposed space. Additive bloom/sky output is also
clamped before fp16 storage, so extreme compensation cannot turn a finite highlight
into infinity in the next pass. Diagnostic material views bypass exposure.

The existing ACES approximation remains the default, with Khronos PBR Neutral as a
selectable alternative. Both output linear Rec.709; the existing output/GUI path
performs the display transfer. The clipping overlay marks exposed channels >= 1 in
red; it indicates values beyond SDR white, not necessarily fp16 overflow or loss after
tonemapping. Feature controls are in Render Features > Tonemap.

The material BRDF is separated into `engine/lighting/shaders/brdf.slang` and uses GGX,
height-correlated Smith visibility, Schlick Fresnel and Lambert diffuse. Perceptual
roughness is squared to obtain alpha. The denominator floor preserves smooth GGX
highlights instead of flattening them. Multi-scattering compensation and its LUT
remain part of the environment-lighting work. The lighting CMake/module registry,
light types and GPU light layout are introduced in L1; L0 changes the existing
camera/forward/post-processing contracts without reversing their dependencies.

References for the equations:

- [Filament shading and exposure](https://google.github.io/filament/Filament.html)
- [Khronos PBR Neutral specification](https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md)

The existing procedural sky LUTs use artist-scaled radiance internally. As a
transitional compatibility calibration, one sky unit corresponds to 5,000 physical
units (the default 20-unit atmosphere sun corresponds to 100,000 lux). Sky-generated
radiance is converted and pre-exposed at composition; atmospheric transmittance and
shadow factors are unchanged. Diffuse environment sampling applies the same
calibration. Linked celestial sun intensity 1 maps to 100,000 lux and moon intensity
0.02 to 0.1 lux. This is not the completed physical atmosphere model in section 10a;
its LUT precision, ground-to-orbit tests and sun attenuation remain separate work.
Existing user-authored artist light values are not silently rescaled on file load.
Legacy scenes without exposure fields load at EV100 15 and may require light/exposure
adjustment. Point-light attenuation is unchanged until the point/spot milestone.

GPU selection first checks compatibility, then applies a pure tested policy:
discrete > integrated > virtual, with device-local heap size breaking ties. CPU and
unknown device types are rejected. `--gpu=<enumeration index|exact name>` overrides
ranking but never compatibility. Startup lists candidates and rejection reasons,
then logs the selected name, driver version, local memory and queue families.
The editor exposes an index/name preference stored in `gpu.txt` beside its executable;
it takes effect on restart, with an explicit command-line override taking precedence.
Both Windows executables export the NVIDIA/AMD high-performance preference symbols.
The existing engine's shader-storage-image/wireframe requirements still apply in
addition to the basic Vulkan 1.3 lighting requirements.

Adapter startup is the only new enumeration work. No GPU waits, CPU per-pixel work
or per-frame device enumeration are introduced by this milestone.
