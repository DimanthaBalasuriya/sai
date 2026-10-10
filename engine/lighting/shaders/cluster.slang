#include "brdf.slang"
#include "lighting.slang"
#include "frame.slang"
struct Push {LightingFrame* frame;uint mode;uint pad;};
[[vk::push_constant]] ConstantBuffer<Push> push;
groupshared uint groupVisible;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[[vk::binding(1,0)]] SamplerState samplers[256];
[shader("compute")]
[numthreads(64,1,1)]
void ComputeMain(uint3 id:SV_DispatchThreadID){
    LightingFrame* f=push.frame;uint total=f->grid.x*f->grid.y*f->grid.z;uint i=id.x;
    if(push.mode==0){
        if(i<f->directionalCount)f->lights[i].shadowIndex=f->camera.w>0&&f->shadows[i].meta.x>0?i:0xffffffff;if(i<f->localCount){uint index=4+i;f->lights[index].shadowIndex=f->camera.w>0&&f->shadows[index].meta.x>0?index:0xffffffff;}
        if(i<f->directionalCount&&f->atmosphere.x>0){GpuLight light=f->lights[i];float mu=-light.direction.y,altitude=f->atmosphere.z;float horizon=-sqrt(saturate(1-pow(f->atmosphere.x/(f->atmosphere.x+altitude),2)));float angularRadius=light.spotScale>0?light.spotScale:.00233;float eclipse=smoothstep(horizon-angularRadius,horizon+angularRadius,mu);float3 transmission=altitude>=f->atmosphere.y?float3(1):textures[0].SampleLevel(samplers[0],float2(max(mu,horizon+.001)*.5+.5,saturate(altitude/f->atmosphere.y)),0).rgb;f->bodyTransmission[i]=float4(transmission*eclipse,0);}
        if(i==0){f->counters[0]=0;f->counters[1]=0;f->counters[2]=0;f->counters[3]=0;}
        if(i>=total)return;
        f->cells[i].offset=i*f->grid.w;f->cells[i].count=0;
        if(push.pad!=0)return;
        uint x=i%f->grid.x,y=(i/f->grid.x)%f->grid.y,z=i/(f->grid.x*f->grid.y);
        float nearZ=exp((float(z)-f->depth.w)/f->depth.z),farZ=exp((float(z+1)-f->depth.w)/f->depth.z);
        float2 lo=float2(x,y)*64/f->screen.xy*2-1,hi=min(float2(x+1,y+1)*64/f->screen.xy,1.f)*2-1;
        // Camera looks down -Z; Vulkan projection flips Y.
        float2 a=float2(lo.x,-hi.y)*f->camera.xy,b=float2(hi.x,-lo.y)*f->camera.xy;
        f->bounds[i].minimum=float4(min(a*nearZ,a*farZ),-farZ,0);
        f->bounds[i].maximum=float4(max(b*nearZ,b*farZ),-nearZ,0);return;
    }
    uint lane=i%64;i/=64;
    if(i>=f->localCount)return;GpuLight light=f->lights[4+i];if((light.flags&1)==0)return;
    float radius=rsqrt(max(light.invRangeSq,1e-20));float3 p=mul(f->view,float4(light.position,1)).xyz;
    float distance=-p.z;if(distance+radius<f->depth.x||distance-radius>f->depth.y)return;
    uint firstZ=DepthSlice(f,distance-radius),lastZ=DepthSlice(f,distance+radius);
    uint2 first=0,last=f->grid.xy-1;
    if(distance>radius){float closest=max(distance-radius,f->depth.x),furthest=distance+radius;
        float2 a=(p.xy-radius)/closest,b=(p.xy+radius)/closest;
        a=min(a,(p.xy-radius)/furthest);b=max(b,(p.xy+radius)/furthest);
        float2 lo=float2(a.x,-b.y)/f->camera.xy*.5+.5,hi=float2(b.x,-a.y)/f->camera.xy*.5+.5;
        if(any(hi<0)||any(lo>1))return;
        first=min(uint2(saturate(lo)*f->screen.xy/64),last);last=min(uint2(saturate(hi)*f->screen.xy/64),last);
    }
    if(lane==0)groupVisible=0;GroupMemoryBarrierWithGroupSync();bool visible=false;
    uint nx=last.x-first.x+1,ny=last.y-first.y+1,nz=lastZ-firstZ+1;
    for(uint candidate=lane;candidate<nx*ny*nz;candidate+=64){uint x=first.x+candidate%nx,y=first.y+(candidate/nx)%ny,z=firstZ+candidate/(nx*ny);
        uint c=(z*f->grid.y+y)*f->grid.x+x;ClusterBounds bounds=f->bounds[c];float3 delta=p-clamp(p,bounds.minimum.xyz,bounds.maximum.xyz);if(dot(delta,delta)>radius*radius)continue;
        uint slot;InterlockedAdd(f->cells[c].count,1,slot);if(slot<f->grid.w)f->indices[c*f->grid.w+slot]=4+i;else InterlockedAdd(f->counters[0],1);
        InterlockedMax(f->counters[1],slot+1);InterlockedAdd(f->counters[2],1);visible=true;
    }
    if(visible)InterlockedOr(groupVisible,1);GroupMemoryBarrierWithGroupSync();if(lane==0&&groupVisible!=0)InterlockedAdd(f->counters[3],1);
}
