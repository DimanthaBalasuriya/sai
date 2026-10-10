uint CubeFace(float3 d){float3 a=abs(d);if(a.x>=a.y&&a.x>=a.z)return d.x>=0?0:1;if(a.y>=a.z)return d.y>=0?2:3;return d.z>=0?4:5;}
float ShadowSample(ShadowRecord r,uint face,float3 position,float3 normal,float3 towardLight){
    float normalBias=r.options.y*(1-saturate(dot(normal,towardLight)));float4 clip=mul(r.matrices[face],float4(position+normal*normalBias,1));float3 projected=clip.xyz/clip.w;
    if(clip.w<=0||any(abs(projected.xy)>1)||projected.z<0||projected.z>1)return 1;
    float4 tile=r.tiles[face];float2 center=(projected.xy*.5+.5)*tile.z+tile.xy;float texel=1.f/4096;float2 lo=tile.xy+texel*.5,hi=tile.xy+tile.z-texel*.5;float total=0;
    float angle=frac(dot(floor(center*4096),float2(.06711056,.00583715)))*6.2831853;float2 axis=float2(cos(angle),sin(angle));uint samples=r.meta.z==0?1:r.meta.z==1?8:16;
    for(uint i=0;i<samples;++i){float a=(i+.5)*2.39996323;float radius=sqrt(float(i)/samples)*(r.meta.z<=1?1.5:2.5);float2 disk=float2(cos(a),sin(a))*radius;float2 rotated=float2(dot(disk,axis),dot(disk,float2(-axis.y,axis.x)));
        float depth=textures[254].SampleLevel(samplers[254],clamp(center+rotated*texel,lo,hi),0).r;
        total+=projected.z+r.options.x>=depth?1:0;
    }return total/samples;
}
float LightVisibility(LightingFrame* f,uint index,GpuLight light,float3 position,float3 normal,out uint cascade){
    cascade=0;if((light.flags&2)==0||f->camera.w==0||light.shadowIndex==0xffffffff)return 1;ShadowRecord r=f->shadows[light.shadowIndex];if(r.meta.x==0)return 1;
    float3 delta=light.position-position;float3 towardLight=light.type==0?-light.direction:normalize(delta);uint face=0;
    if(light.type==0){float z=-mul(f->view,float4(position,1)).z;while(face+1<r.meta.x&&z>r.splits[face])++face;cascade=face;if(z>r.splits[r.meta.x-1])return 1;
        float visibility=ShadowSample(r,face,position,normal,towardLight);float previous=face==0?f->depth.x:r.splits[face-1];float blend=max((r.splits[face]-previous)*r.options.z,.001);
        if(face+1<r.meta.x&&z>r.splits[face]-blend)visibility=lerp(visibility,ShadowSample(r,face+1,position,normal,towardLight),saturate((z-r.splits[face]+blend)/blend));
        float fade=saturate((r.splits[r.meta.x-1]-z)/max(1,r.options.w*.1));return lerp(1,visibility,fade);
    }
    if(light.type==1)face=CubeFace(position-light.position);return ShadowSample(r,face,position,normal,towardLight);
}
