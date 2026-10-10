struct ExposureState {uint histogram[256];float ev;uint valid;float target;float pad;};
struct Push {ExposureState* state;uint mode;uint width;uint height;float exposure;float oldEv;float minEv;float maxEv;float speedUp;float speedDown;float dt;};
[[vk::push_constant]] ConstantBuffer<Push> push;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[shader("compute")][numthreads(16,16,1)]
void ComputeMain(uint3 id:SV_DispatchThreadID){
    uint linear=id.y*16+id.x;
    if(push.mode==0){if(id.y==0&&id.x<256)push.state->histogram[id.x]=0;return;}
    if(push.mode==1){if(id.x>=push.width||id.y>=push.height)return;float3 c=textures[0].Load(int3(id.xy,0)).rgb/max(push.exposure,1e-12);float l=dot(c,float3(.2126,.7152,.0722));uint bin=uint(clamp((log2(max(l,1e-8))+16)*256/48,0,255));InterlockedAdd(push.state->histogram[bin],1);return;}
    if(any(id!=0))return;
    uint total=push.width*push.height;float low=total*.02,high=total*.98,sum=0,count=0;uint prefix=0;
    for(uint i=0;i<256;++i){uint n=push.state->histogram[i];float weight=max(0,min(float(prefix+n),high)-max(float(prefix),low));sum+=(-16+(float(i)+.5)*48/256)*weight;count+=weight;prefix+=n;}
    float target=clamp(sum/max(count,1)+log2(100.0/12.5),push.minEv,push.maxEv);float speed=target>push.oldEv?push.speedUp:push.speedDown;
    push.state->target=target;push.state->ev=lerp(push.oldEv,target,1-exp(-speed*push.dt));push.state->valid=1;
}
