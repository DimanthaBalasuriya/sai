struct DrawData {
    float4x4 mvp;
    uint textureIndex;
    uint samplerIndex;
};
[[vk::push_constant]] ConstantBuffer<DrawData> drawData;
[[vk::binding(0, 0)]] Texture2D<float4> textures[16];
[[vk::binding(1, 0)]] SamplerState samplers[16];
struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
};
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
[shader("vertex")]
VertexOutput VertexMain(VertexInput input) {
    VertexOutput output;
    output.position = mul(drawData.mvp, float4(input.position, 1));
    output.uv = input.uv;
    return output;
}
[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target {
    return textures[drawData.textureIndex].Sample(samplers[drawData.samplerIndex], input.uv);
}
