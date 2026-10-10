float RangeAttenuation(GpuLight light,float distanceSquared){float window=saturate(1-pow(distanceSquared*light.invRangeSq,2));return window*window/max(distanceSquared,.0001);}
float3 EvaluatePoint(GpuLight light,float3 position,float3 n,float3 v,float3 base,float metallic,float roughness){
    if((light.flags&1)==0)return 0;float3 delta=light.position-position;float d2=dot(delta,delta);
    return Brdf(n,v,delta*rsqrt(max(d2,1e-12)),base,metallic,roughness)*light.color*RangeAttenuation(light,d2);
}
