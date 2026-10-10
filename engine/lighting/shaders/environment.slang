struct Push {float4* sh;uint mode;uint size;float roughness;uint hasTexture;float4 ambient;};
[[vk::push_constant]] ConstantBuffer<Push> push;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[[vk::binding(1,0)]] SamplerState samplers[256];
[[vk::binding(2,0)]] RWTexture2D<float4> outputs[8];
[[vk::binding(6,0)]] RWTexture2DArray<float4> cubes[8];
float3 Radiance(float3 d){if(push.hasTexture!=0){float2 uv=float2(atan2(d.z,d.x)/(2*3.14159265)+.5,acos(clamp(d.y,-1,1))/3.14159265);return textures[0].SampleLevel(samplers[0],uv,0).rgb;}return push.ambient.rgb*lerp(.2,2,saturate(d.y*.5+.5));}
float SH(uint i,float3 d){if(i==0)return .282095;if(i==1)return .488603*d.y;if(i==2)return .488603*d.z;if(i==3)return .488603*d.x;if(i==4)return 1.092548*d.x*d.y;if(i==5)return 1.092548*d.y*d.z;if(i==6)return .315392*(3*d.z*d.z-1);if(i==7)return 1.092548*d.x*d.z;return .546274*(d.x*d.x-d.y*d.y);}
float2 Hammersley(uint i,uint n){return float2(float(i)/n,float(reversebits(i))*2.3283064365386963e-10);}
float3 GGX(float2 u,float roughness,float3 n){float a=roughness*roughness,phi=6.2831853*u.x,cosine=sqrt((1-u.y)/(1+(a*a-1)*u.y));float sine=sqrt(max(0,1-cosine*cosine));float3 h=float3(cos(phi)*sine,sin(phi)*sine,cosine),up=abs(n.z)<.999?float3(0,0,1):float3(1,0,0);float3 t=normalize(cross(up,n)),b=cross(n,t);return normalize(t*h.x+b*h.y+n*h.z);}
float3 FaceDirection(uint face,float2 uv){float2 p=uv*2-1;if(face==0)return normalize(float3(1,-p.y,-p.x));if(face==1)return normalize(float3(-1,-p.y,p.x));if(face==2)return normalize(float3(p.x,1,p.y));if(face==3)return normalize(float3(p.x,-1,-p.y));if(face==4)return normalize(float3(p.x,-p.y,1));return normalize(float3(-p.x,-p.y,-1));}
[shader("compute")][numthreads(8,8,1)]
void ComputeMain(uint3 id:SV_DispatchThreadID){
    if(push.mode==0){if(id.y!=0||id.z!=0||id.x>=9)return;float3 sum=0;for(uint i=0;i<2048;++i){float2 u=Hammersley(i,2048);float z=1-2*u.x,r=sqrt(max(0,1-z*z)),phi=6.2831853*u.y;float3 d=float3(r*cos(phi),z,r*sin(phi));sum+=Radiance(d)*SH(id.x,d);}float convolution=id.x==0?3.14159265:id.x<4?2.0943951:.78539816;push.sh[id.x]=float4(sum*(12.5663706/2048)*convolution,0);return;}
    if(id.x>=push.size||id.y>=push.size)return;float2 uv=(float2(id.xy)+.5)/push.size;
    if(push.mode==1){float3 n=FaceDirection(id.z,uv),sum=0;float weight=0;for(uint i=0;i<64;++i){float3 h=GGX(Hammersley(i,64),max(.01,push.roughness),n),l=normalize(2*dot(n,h)*h-n);float nl=saturate(dot(n,l));sum+=Radiance(l)*nl;weight+=nl;}cubes[0][id]=float4(sum/max(weight,1e-6),1);return;}
    float nv=max(uv.x,.001),roughness=uv.y;float3 v=float3(sqrt(1-nv*nv),0,nv);float A=0,B=0;
    for(uint i=0;i<128;++i){float3 h=GGX(Hammersley(i,128),roughness,float3(0,0,1)),l=normalize(2*dot(v,h)*h-v);float nl=saturate(l.z),nh=saturate(h.z),vh=saturate(dot(v,h));if(nl>0){float k=roughness*roughness*.5;float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));float visible=G*vh/max(nh*nv,1e-6),fc=pow(1-vh,5);A+=(1-fc)*visible;B+=fc*visible;}}
    outputs[0][id.xy]=float4(A/128,B/128,0,1);
}
