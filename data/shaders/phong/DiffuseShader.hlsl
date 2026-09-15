// Basic ambient + Lambert material. Texture and tint are per-material parameters.
#include "Surface.hlsli"

float4 PSMain(PS_IN input) : SV_Target
{
    float ndotl = saturate(dot(normalize(input.worldNormal), normalize(-LightDir)));
    float4 albedo = SampleAlbedo(input);
    float shadow = ShadowFactor(input.worldPos);
    float3 lighting = (MaterialAmbient + MaterialDiffuse * ndotl * shadow) * LightColor * LightIntensity;
    return SurfaceOutput(lighting * albedo.rgb, albedo.a);
}
