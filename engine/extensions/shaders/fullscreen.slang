struct PassData { float4x4 inverseViewProjection;float4 parameters;uint4 flags;float4 camera; };
[[vk::push_constant]] ConstantBuffer<PassData> pass;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[[vk::binding(1,0)]] SamplerState samplers[256];
struct VertexOutput { float4 position:SV_Position;float2 uv:TEXCOORD0; };
[shader("vertex")]
VertexOutput VertexMain(uint vertex:SV_VertexID) {
    VertexOutput output;float2 uv=float2((vertex<<1)&2,vertex&2);
    output.position=float4(uv*2-1,0,1);output.uv=uv;return output;
}
