# ADR 0015: Shadow filtering and bias

Use reverse-Z D32 depth for sphere-fit directional cascades and local atlas tiles.
Cascade XY origins snap to shadow texels; practical uniform/logarithmic splits
and a configurable overlap reduce transitions. Point lights use six 90° views;
spots use twice the authored outer half angle. Alpha-mask materials cast cutouts.

Filtering compares point-sampled depths with 1, 8 or 16 rotated disk taps, clamped
inside each tile. A negative raster offset moves reverse-Z caster depth away from
receivers. A positive receiver depth bias and a normal offset scaled by grazing
angle are independently editable. The bias overlay shows visibility in red/green
and the grazing-angle factor in blue. The atlas viewer uses log-scaled depth.

Cache keys contain the light identity, matrix, tile and spatially queried caster
identities/transforms/mesh/material. Each fenced atlas keeps its own keys. Disabled
and unallocated faces are invalidated; changing a light or entering/leaving caster
updates only affected tiles. Static settled frames issue zero shadow depth passes.

Reference: [Microsoft cascaded shadow maps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps).
