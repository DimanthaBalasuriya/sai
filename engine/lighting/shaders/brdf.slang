// GGX / correlated Smith / Schlick / Lambert. AO is applied only to indirect light.
float3 Brdf(float3 n,float3 v,float3 l,float3 base,float metallic,float roughness) {
    float3 h=(v+l)*rsqrt(max(dot(v+l,v+l),1e-12)); float nl=saturate(dot(n,l)),nv=max(saturate(dot(n,v)),0.001),nh=saturate(dot(n,h)),vh=saturate(dot(v,h));
    float alpha=roughness*roughness,alpha2=alpha*alpha;
    float denominator=nh*nh*(alpha2-1)+1;
    float distribution=alpha2/max(3.14159265*denominator*denominator,1e-10);
    // Height-correlated Smith visibility, with perceptual roughness squared.
    float visibility=0.5/max(nl*sqrt(nv*nv*(1-alpha2)+alpha2)+nv*sqrt(nl*nl*(1-alpha2)+alpha2),1e-7);
    float3 f0=lerp(float3(0.04),base,metallic);
    float3 fresnel=f0+(1-f0)*pow(1-vh,5);
    float3 specular=distribution*visibility*fresnel;
    return ((1-fresnel)*(1-metallic)*base/3.14159265+specular)*nl;
}
