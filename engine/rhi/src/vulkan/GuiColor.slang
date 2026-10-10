// ImGui vertex RGB is an authored sRGB byte color. Decode after interpolation,
// before the sRGB attachment encodes the result. Texture samples are already linear.
[[vk::binding(0,0)]] Sampler2D<float4> image;
struct Input { [[vk::location(1)]] float2 uv:TEXCOORD0; [[vk::location(0)]] float4 color:COLOR0; };
[shader("fragment")]
float4 main(Input input):SV_Target {
    float3 rgb=input.color.rgb;
    float3 linear=select(rgb<=0.04045,rgb/12.92,pow((rgb+0.055)/1.055,2.4));
    return float4(linear,input.color.a)*image.Sample(input.uv);
}
