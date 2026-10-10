struct ClusterBounds {float4 minimum;float4 maximum;};
struct ClusterCell {uint offset;uint count;};
struct ShadowRecord {float4x4 matrices[6];float4 tiles[6];float4 splits;float4 options;uint4 meta;};
struct AreaShape {float4 rightWidth;float4 upHeight;};
struct LtcEntry {float4 matrix;float4 amplitude;};
struct LightingFrame {
    float4x4 view;float4 camera;uint4 grid;float4 depth;float4 screen;
    GpuLight* lights;ClusterBounds* bounds;ClusterCell* cells;uint* indices;uint* counters;
    uint localCount;uint directionalCount;ShadowRecord* shadows;
    AreaShape* areas;LtcEntry* ltc;
    uint2 bodyPadding;float4 bodyTransmission[4];
    float4 atmosphere;
};
uint DepthSlice(LightingFrame* f,float z){return uint(clamp(floor(log(max(z,f->depth.x))*f->depth.z+f->depth.w),0.f,float(f->grid.z-1)));}
uint ClusterIndex(LightingFrame* f,float2 pixel,float3 position){uint2 tile=min(uint2(pixel)/64,f->grid.xy-1);uint z=DepthSlice(f,-mul(f->view,float4(position,1)).z);return (z*f->grid.y+tile.y)*f->grid.x+tile.x;}
