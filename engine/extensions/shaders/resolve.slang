#include "fullscreen.slang"
[shader("fragment")]
float4 FragmentMain(VertexOutput input):SV_Target { return float4(saturate(textures[0].Sample(samplers[0],input.uv).rgb),1); }
