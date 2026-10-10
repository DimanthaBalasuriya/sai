#include "../../shaders/fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target {
    float4 geometry=textures[0].Sample(samplers[0],input.uv);
    if (geometry.a>0.5) return geometry;
    float4 far=mul(pass.inverseViewProjection,float4(input.uv*2-1,1,1));
    float3 direction=normalize(far.xyz/far.w-pass.camera.xyz);
    float horizon=pow(1-saturate(abs(direction.y)),4);
    float3 sky=lerp(float3(0.08,0.22,0.48),float3(0.5,0.68,0.82),horizon);
    if (direction.y<0) sky=lerp(float3(0.06,0.075,0.09),float3(0.32,0.38,0.43),horizon);
    float sun=pow(saturate(dot(direction,normalize(float3(-0.45,0.65,-0.6)))),512);
    sky+=float3(12,9,5)*sun;
    return float4(clamp(sky*pass.parameters.x,0,65504),1);
}
