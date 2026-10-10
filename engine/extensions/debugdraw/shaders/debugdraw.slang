struct DrawData { float4x4 viewProjection; };
[[vk::push_constant]] ConstantBuffer<DrawData> draw;
struct VertexInput { [[vk::location(0)]] float3 position:POSITION;[[vk::location(1)]] float3 color:COLOR0; };
struct VertexOutput { float4 position:SV_Position;float3 color:COLOR0; };
[shader("vertex")]
VertexOutput VertexMain(VertexInput input) { VertexOutput output;output.position=mul(draw.viewProjection,float4(input.position,1));output.color=input.color;return output; }
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target { return float4(input.color,1); }
