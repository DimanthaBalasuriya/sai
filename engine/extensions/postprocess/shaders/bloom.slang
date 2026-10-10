#include "../../shaders/fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target {
    float3 color=textures[0].Sample(samplers[0],input.uv).rgb;
    if(pass.flags.x==0) { float peak=max(color.r,max(color.g,color.b));return float4(color*max(peak-pass.parameters.x,0)/max(peak,0.00001),1); }
    if(pass.flags.x==3) return float4(min(color+textures[1].Sample(samplers[0],input.uv).rgb*pass.parameters.y,65504),1);
    float2 step=pass.flags.x==1?float2(pass.parameters.z,0):float2(0,pass.parameters.w);
    float3 blurred=color*0.227027;
    blurred+=(textures[0].Sample(samplers[0],input.uv+step*1.384615).rgb+textures[0].Sample(samplers[0],input.uv-step*1.384615).rgb)*0.316216;
    blurred+=(textures[0].Sample(samplers[0],input.uv+step*3.230769).rgb+textures[0].Sample(samplers[0],input.uv-step*3.230769).rgb)*0.070270;
    return float4(blurred,1);
}
