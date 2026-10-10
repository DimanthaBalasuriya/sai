struct Material {float4 base;float4 factors;float4 emissiveAlpha;uint4 textures;uint4 extra;};
struct Push {float4x4 matrix;float4x4* models;Material* materials;uint index;uint material;uint2 pad;};
[[vk::push_constant]] ConstantBuffer<Push> push;
[[vk::binding(0,0)]] Texture2D<float4> textures[256];
[[vk::binding(1,0)]] SamplerState samplers[256];
struct Output {float4 position:SV_Position;float2 uv:TEXCOORD0;};
[shader("vertex")]
Output VertexMain([[vk::location(0)]] float3 position:POSITION,[[vk::location(1)]] float2 uv:TEXCOORD0){Output o;o.position=mul(push.matrix,mul(push.models[push.index],float4(position,1)));o.uv=uv;return o;}
[shader("fragment")]
void FragmentMain(Output input){Material m=push.materials[push.material];if(m.extra.y==1){float alpha=m.base.a;if(m.textures.x!=0xffffffff)alpha*=textures[m.textures.x].Sample(samplers[0],input.uv).a;clip(alpha-m.emissiveAlpha.w);}}
