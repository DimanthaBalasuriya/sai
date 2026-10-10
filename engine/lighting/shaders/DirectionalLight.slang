float3 EvaluateDirectional(GpuLight light,float3 normal,float3 view,float3 base,float metallic,float roughness){
    if((light.flags&1)==0)return 0;
    return Brdf(normal,view,normalize(-light.direction),base,metallic,roughness)*light.color;
}
