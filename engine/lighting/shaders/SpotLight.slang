float3 EvaluateSpot(GpuLight light,float3 position,float3 n,float3 v,float3 base,float metallic,float roughness){
    float3 delta=position-light.position;float cosine=dot(delta*rsqrt(max(dot(delta,delta),1e-12)),light.direction);
    float cone=saturate(cosine*light.spotScale+light.spotOffset);return EvaluatePoint(light,position,n,v,base,metallic,roughness)*cone*cone;
}
