# ADR 0016: Environment, fog, exposure and LTC

EnvironmentSettings selects ConstantAmbient, HDRI, or the procedural sky. Module
settings extract into owned render snapshots; renderer code holds no ECS pointers.
IEnvironmentLighting and ISceneFogProvider define the provider contracts.

HDRI files load to linear fp16 equirectangular textures. Compute projects SH9,
prefilters five GGX roughness levels into separate cubes, and integrates a BRDF LUT.
Separate roughness cubes fit the current image API without changing mip storage.
Shading interpolates adjacent levels. Recompute only after source/ambient changes.
The synthetic studio fixture is authored in nits and requires no external asset.

Height fog analytically integrates exponential density over a depth-limited ray.
The existing environment shadow texture/matrix is the sun-occlusion slot used by
the procedural cloud provider. Exposure uses a 256-bin GPU log-luminance histogram,
2–98% trimming, configurable EV limits/speeds, and exponential adaptation. Fenced
readback supplies the next exposed frame and the editor histogram; rendering does
not wait for a new histogram. Diagnostic image comparison explicitly waits outside
frames and is only used by the verification command.

Rect/disk lights use one-sided radiance in nits, or lumens divided by pi times
emitting area. Artist value 1 means 1000 nit. The disk is a 16-sided polygon. LTC
uses the authors' 64² inverse-matrix/magnitude fit with bilinear lookup and horizon
clipping. Schlick view Fresnel weights specular. These types reuse clustered bounds;
area-light shadows are not part of the LTC milestone.

LTC data and license: [authors' repository](https://github.com/selfshadow/ltc_code),
`engine/lighting/third_party/ltc/LICENSE`. Reference: Heitz, Dupuy, Hill and Neubelt,
*Real-Time Polygonal-Light Shading with Linearly Transformed Cosines*, SIGGRAPH 2016.
