#include "common.slang"
#include "aurora-radiance.slang"
struct PassData {float4x4 inverseViewProjection;float4 parameters;Parameters* settings;uint2 flags;float4 camera;};
[[vk::push_constant]] ConstantBuffer<PassData> pass;
struct VertexOutput{float4 position:SV_Position;float2 uv:TEXCOORD0;};
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target{
    Parameters p=*pass.settings;float4 scene=images[0].SampleLevel(samplers[0],input.uv,0);
    if(scene.a==0)scene.rgb+=AuroraRadiance(ViewRay(p,input.uv),p.cameraTime.w,p.moonColor.w)*pass.parameters.w;
    scene.rgb=clamp(scene.rgb,0,65504);return scene;
}
