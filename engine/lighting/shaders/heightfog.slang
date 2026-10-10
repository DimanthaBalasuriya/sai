#include "../../extensions/shaders/fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target{
    float4 source=textures[0].SampleLevel(samplers[0],input.uv,0);float depth=textures[1].SampleLevel(samplers[0],input.uv,0).r;
    float4 point=mul(pass.inverseViewProjection,float4(input.uv*2-1,max(depth,1e-7),1));float3 relative=point.xyz/max(abs(point.w),1e-7);float distance=min(length(relative),pass.parameters.w);float dy=relative.y*distance/max(length(relative),1e-5);
    float falloff=pass.parameters.y,baseDensity=pass.parameters.x*exp(clamp(-falloff*(pass.camera.w-pass.parameters.z),-20,20));float x=falloff*dy;
    float integral=abs(x)<.001?1-x*.5:(1-exp(clamp(-x,-20,20)))/x;float transmittance=exp(-min(80,baseDensity*distance*max(0,integral)));
    return float4(source.rgb*transmittance+pass.camera.rgb*(1-transmittance),source.a);
}
