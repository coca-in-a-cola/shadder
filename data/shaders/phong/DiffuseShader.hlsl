// Basic ambient + Lambert material. Texture and tint are per-material parameters.
#include "Surface.hlsli"

float4 PSMain(PS_IN input) : SV_Target
{
    float ndotl = saturate(dot(normalize(input.worldNormal), normalize(-LightDir)));
    float4 albedo = SampleAlbedo(input);
    return SurfaceOutput(DiffuseLighting(ndotl) * albedo.rgb, albedo.a);
}
