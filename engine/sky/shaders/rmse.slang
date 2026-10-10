struct Push {float4* values;uint mode;uint width;uint height;uint count;};
[[vk::push_constant]] ConstantBuffer<Push> push;
[[vk::binding(0,0)]] Texture2D<float4> images[256];
groupshared float3 sums[64];
[shader("compute")][numthreads(64,1,1)]
void ComputeMain(uint3 id:SV_DispatchThreadID,uint lane:SV_GroupIndex,uint3 group:SV_GroupID){
    if(push.mode==1){if(id.x!=0)return;float3 sum=0;for(uint i=0;i<push.count;++i)sum+=push.values[i+1].rgb;push.values[0]=float4(sqrt(sum/float(push.width*push.height)),1);return;}
    float3 delta=0;if(id.x<push.width*push.height)delta=images[0].Load(int3(id.x%push.width,id.x/push.width,0)).rgb;sums[lane]=delta*delta;GroupMemoryBarrierWithGroupSync();
    for(uint stride=32;stride>0;stride/=2){if(lane<stride)sums[lane]+=sums[lane+stride];GroupMemoryBarrierWithGroupSync();}if(lane==0)push.values[group.x+1]=float4(sums[0],0);
}
