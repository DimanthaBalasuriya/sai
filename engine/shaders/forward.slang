#include "../lighting/shaders/brdf.slang"
#include "../lighting/shaders/lighting.slang"
#include "../lighting/shaders/frame.slang"
struct Instance { float4x4 model; float4x4 normal; float4 color; };
struct PointLight { float4 positionRange; float4 colorIntensity; };
struct SceneData { float4x4 viewProjection; PointLight directional[4]; PointLight points[16]; uint4 counts; float4 cameraPosition; float4x4 worldToShadow; float4 environment; GpuLight* lights;uint directionalCount;uint localCount; LightingFrame* lighting;float4* irradiance;float4 ambient; };
struct Material { float4 baseColor; float4 factors; float4 emissiveAlpha; uint4 textures; uint4 extra; };
struct DrawData { SceneData* scene; Instance* instances; Material* materials; uint materialIndex; uint debugView; };
[[vk::push_constant]] ConstantBuffer<DrawData> draw;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[[vk::binding(1,0)]] SamplerState samplers[256];
[[vk::binding(4,0)]] TextureCube<float4> environments[8];
#include "../lighting/shaders/shadows.slang"
#include "../lighting/shaders/AreaLight.slang"
struct VertexInput {
    [[vk::location(0)]] float3 position:POSITION;
    [[vk::location(1)]] float2 uv:TEXCOORD0;
    [[vk::location(2)]] float3 normal:NORMAL;
    uint instance:SV_InstanceID;
};
struct VertexOutput { float4 position:SV_Position; float3 worldPosition:TEXCOORD0; float3 normal:TEXCOORD1; float2 uv:TEXCOORD2; float3 color:TEXCOORD3; };
[shader("vertex")]
VertexOutput VertexMain(VertexInput input) {
    Instance instance=draw.instances[input.instance];
    float4 world=mul(instance.model,float4(input.position,1));
    VertexOutput output;
    float3 relative=mul(instance.model,float4(0,0,0,1)).xyz-draw.scene->cameraPosition.xyz;
    float3 rotated=mul((float3x3)instance.model,input.position);
    output.position=mul(draw.scene->viewProjection,float4(relative+rotated,1));
    output.worldPosition=world.xyz; output.normal=mul((float3x3)instance.normal,input.normal);
    output.uv=input.uv; output.color=instance.color.xyz; return output;
}
[shader("fragment")]
float4 FragmentMain(VertexOutput input,bool front:SV_IsFrontFace):SV_Target {
    Material material=draw.materials[draw.materialIndex];
    float4 base=material.baseColor*float4(input.color,1);
    if(material.textures.x!=0xffffffff) base*=textures[material.textures.x].Sample(samplers[0],input.uv);
    if(material.extra.y==1) clip(base.a-material.emissiveAlpha.w);
    float metallic=material.factors.x,roughness=material.factors.y;
    if(material.textures.y!=0xffffffff) { float4 mr=textures[material.textures.y].Sample(samplers[0],input.uv); metallic*=mr.b;roughness*=mr.g; }
    roughness=lerp(roughness,max(.045,roughness*.4),saturate(draw.scene->environment.w));roughness=clamp(roughness,0.045,1); metallic=saturate(metallic);
    float3 normal=normalize(input.normal);
    if(material.extra.z!=0&&!front) normal=-normal;
    if(material.textures.z!=0xffffffff) {
        float3 mapped=textures[material.textures.z].Sample(samplers[0],input.uv).xyz*2-1; mapped.xy*=material.factors.z;
        float3 q1=ddx(input.worldPosition),q2=ddy(input.worldPosition); float2 uv1=ddx(input.uv),uv2=ddy(input.uv);
        float determinant=uv1.x*uv2.y-uv1.y*uv2.x;
        if(abs(determinant)>0.000001) {
            float3 tangent=normalize((q1*uv2.y-q2*uv1.y)/determinant);
            tangent=normalize(tangent-normal*dot(normal,tangent));
            float3 bitangent=normalize((-q1*uv2.x+q2*uv1.x)/determinant);
            normal=normalize(tangent*mapped.x+bitangent*mapped.y+normal*mapped.z);
        }
    }
    if(draw.debugView==1) return float4(base.rgb,1);
    if(draw.debugView==2) return float4(normal*0.5+0.5,1);
    if(draw.debugView==3) return float4(float3(input.position.z),1);
    if(draw.debugView==4) return float4(0.3,0.85,1,1);
    if(draw.debugView==6)return float4(roughness.xxx,1);
    if(draw.debugView==7)return float4(metallic.xxx,1);
    if(draw.debugView==5)base.rgb=1;
    float occlusion=1;
    if(material.extra.x!=0xffffffff) occlusion=lerp(1,textures[material.extra.x].Sample(samplers[0],input.uv).r,material.factors.w);
    if(draw.debugView==8)return float4(occlusion.xxx,1);
    float3 view=normalize(draw.scene->cameraPosition.xyz-input.worldPosition);
    float3 color=base.rgb*(1-metallic)*draw.scene->ambient.rgb*occlusion*draw.scene->environment.z;
    if(draw.scene->environment.x>0) color=base.rgb*(1-metallic)*environments[0].SampleLevel(samplers[0],normal,0).rgb*occlusion*draw.scene->environment.z;
    if(draw.scene->ambient.w>0){float3 n=normal;float basis[9]={.282095,.488603*n.y,.488603*n.z,.488603*n.x,1.092548*n.x*n.y,1.092548*n.y*n.z,.315392*(3*n.z*n.z-1),1.092548*n.x*n.z,.546274*(n.x*n.x-n.y*n.y)};float3 irradiance=0;for(uint c=0;c<9;++c)irradiance+=draw.scene->irradiance[c].rgb*basis[c];float3 F0=lerp(float3(.04),base.rgb,metallic);float nv=saturate(dot(n,view));float3 F=F0+(max(float3(1-roughness),F0)-F0)*pow(1-nv,5);float3 reflection=reflect(-view,n);float level=roughness*4;uint lo=min(4,uint(level)),hi=min(4,lo+1);float3 spec=lerp(environments[1+lo].SampleLevel(samplers[0],reflection,0).rgb,environments[1+hi].SampleLevel(samplers[0],reflection,0).rgb,frac(level));float2 brdf=textures[253].SampleLevel(samplers[253],float2(nv,roughness),0).rg;color=((1-F)*(1-metallic)*base.rgb*max(irradiance,0)/3.14159265+spec*(F0*brdf.x+brdf.y))*occlusion*draw.scene->environment.z;}
    float shadow=1;
    if(draw.scene->environment.y>0){float2 uv=mul(draw.scene->worldToShadow,float4(input.worldPosition,1)).xy;if(all(uv>=0)&&all(uv<=1))shadow=textures[255].SampleLevel(samplers[0],uv,0).r;}
    // Counts select the physical buffer without a 64-bit pointer comparison.
    if(draw.scene->directionalCount+draw.scene->localCount>0){
        if(draw.debugView==11&&draw.scene->lighting->camera.w>0)return float4(textures[254].SampleLevel(samplers[0],input.position.xy/draw.scene->lighting->screen.xy,0).rrr,1);
        uint cascade=0;for(uint d=0;d<draw.scene->directionalCount;++d){GpuLight light=draw.scene->lights[d];light.color*=draw.scene->lighting->bodyTransmission[d].rgb;float visibility=LightVisibility(draw.scene->lighting,d,light,input.worldPosition,normal,cascade);color+=EvaluateDirectional(light,normal,view,base.rgb,metallic,roughness)*visibility*(d==0?shadow:1);if(draw.debugView==12&&d==0)return float4(1-visibility,visibility,saturate(1-dot(normal,-light.direction)),1);if(draw.debugView==10&&d==0){float3 palette[4]={float3(1,.2,.2),float3(.2,1,.2),float3(.2,.2,1),float3(1,1,.2)};return float4(palette[cascade],1);}}
        if(draw.debugView==10){float3 colors[4]={float3(1,.2,.2),float3(.2,1,.2),float3(.2,.2,1),float3(1,1,.2)};return float4(colors[cascade],1);}
        LightingFrame* f=draw.scene->lighting;
        uint cluster=ClusterIndex(f,input.position.xy,input.worldPosition);ClusterCell cell=f->cells[cluster];
        if(draw.debugView==9)return float4(cell.count>f->grid.w?float3(1,0,1):lerp(float3(0,0,.2),float3(1,.2,0),saturate(float(cell.count)/32)),1);
        bool clustered=(f->screen.z>1.5||(f->screen.z<.5&&f->localCount>16))&&cell.count<=f->grid.w;
        uint count=clustered?cell.count:f->localCount;
        for(uint i=0;i<count;++i){GpuLight light=f->lights[clustered?f->indices[cell.offset+i]:4+i];
            uint lightIndex=clustered?f->indices[cell.offset+i]:4+i;uint ignored;float visibility=LightVisibility(f,lightIndex,light,input.worldPosition,normal,ignored);
            if(light.type>=3)color+=EvaluateArea(f,lightIndex,light,input.worldPosition,normal,view,base.rgb,metallic,roughness);
            else if(light.type==2)color+=visibility*EvaluateSpot(light,input.worldPosition,normal,view,base.rgb,metallic,roughness);
            else color+=visibility*EvaluatePoint(light,input.worldPosition,normal,view,base.rgb,metallic,roughness);
        }    }else{
    for(uint d=0;d<draw.scene->counts.y;++d) color+=Brdf(normal,view,normalize(-draw.scene->directional[d].positionRange.xyz+float3(0.000001)),base.rgb,metallic,roughness)*draw.scene->directional[d].colorIntensity.xyz*(d==0?shadow:1);
    for(uint i=0;i<draw.scene->counts.x;++i) {
        PointLight point=draw.scene->points[i];
        float3 delta=point.positionRange.xyz-input.worldPosition;
        float distanceSquared=max(dot(delta,delta),0.01);
        float distance=sqrt(distanceSquared);
        float attenuation=pow(saturate(1-distance/point.positionRange.w),2)*point.colorIntensity.w/(1+distanceSquared*0.002);
        color+=Brdf(normal,view,delta/distance,base.rgb,metallic,roughness)*point.colorIntensity.xyz*attenuation;
    }
    }
    float3 emissive=material.emissiveAlpha.xyz;
    if(material.textures.w!=0xffffffff) emissive*=textures[material.textures.w].Sample(samplers[0],input.uv).rgb;
    color+=emissive;
    // Expose in fp32 before the fp16 attachment; clamp out-of-range highlights.
    color=min(max(color*draw.scene->cameraPosition.w,0),65504);
#ifdef ENGINE_HDR_OUTPUT
    return float4(color,1);
#else
    return float4(color/(1+color),1);
#endif
}

