#include "../../shaders/fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target {
    float3 color=max(textures[0].Sample(samplers[0],input.uv).rgb*pass.parameters.x,0);
    if(pass.flags.y!=0 && any(color>=1))return float4(1,0,0,1);
    // Khronos PBR Neutral specification: linear Rec.709 in/out.
    if(pass.flags.x==1){
        float minimum=min(color.r,min(color.g,color.b));
        color-=minimum<=0.08 ? minimum-minimum*minimum/0.16 : 0.04;
        float peak=max(color.r,max(color.g,color.b));
        if(peak<=0.76)return float4(color,1);
        float compressed=1-0.0576/(peak-0.52);
        float saturation=1/(1+0.15*(peak-compressed));
        return float4(lerp(float3(compressed),color*(compressed/peak),saturation),1);
    }
    return float4(saturate((color*(2.51*color+0.03))/(color*(2.43*color+0.59)+0.14)),1);
}
