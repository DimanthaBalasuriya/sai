struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
};
[shader("vertex")]
VertexOutput VertexMain(uint vertexId : SV_VertexID) {
    const float2 positions[3] = { float2(0.0, -0.65), float2(0.65, 0.65), float2(-0.65, 0.65) };
    const float3 colors[3] = { float3(1.0, 0.25, 0.15), float3(0.15, 1.0, 0.35), float3(0.15, 0.4, 1.0) };
    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.color = colors[vertexId];
    return output;
}
[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target { return float4(input.color, 1.0); }
