#include "../../extensions/shaders/fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target{float depth=textures[0].SampleLevel(samplers[0],input.uv,0).r;float value=log2(1+1000*depth)/log2(1001);float2 grid=frac(input.uv*16);if(any(grid<.01))return float4(.1,.2,.4,1);return float4(value.xxx,1);}
