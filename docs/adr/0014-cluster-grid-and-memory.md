# ADR 0014: Cluster grid and memory

64 x 64 pixel tiles and 24 logarithmic depth slices cover the camera near/far
range. Projection changes rebuild view-space bounds; other frames only clear
cell counts. One 64-lane workgroup handles each local light, tests its conservative
range sphere against candidate AABBs, and appends indices with atomics. Spot and
area lights share those bounds without special cases in the cluster core.

Auto uses a loop for 16 or fewer local lights. The editor can force either path.
Each cluster reserves up to 256 indices. `Cluster lists MiB per frame` defaults
to 16 and lowers that capacity at larger resolutions. Overflow uses the full
loop for the affected pixel, preserving lighting rather than dropping lights.
The heatmap marks overflow in magenta; counters are read from fenced frame slots.

The shadow atlas has a separate fixed limit: 4096² D32 per frame slot, 128 MiB
across two slots. Tile priorities, resolution reduction, a local-light budget,
and all-or-nothing six-face point allocations bound work and avoid partial shadows.
