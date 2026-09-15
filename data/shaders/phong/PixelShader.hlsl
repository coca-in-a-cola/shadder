// PixelShader.hlsl — Phong pixel shader (модель Фонга, не Blinn).
//
// Формула: ambient + diffuse * max(dot(N, L), 0) + specular * pow(max(dot(R, V), 0), shininess)
//   N — нормализованная мировая нормаль (интерполирована, нормализуется в PS),
//   L — направление из точки к свету = -LightDir (свет направленный, LightDir задаёт
//       направление РАСПРОСТРАНЕНИЯ света, т.е. «куда светит»),
//   R — отражённый вектор L относительно N: R = reflect(-L, N),
//   V — направление из точки к наблюдателю = normalize(CameraPos - worldPos).
//   ПОЗИЦИЯ ЗРИТЕЛЯ ОБЯЗАТЕЛЬНА В ПИКСЕЛЬНОМ ШЕЙДЕРЕ: CameraPos берётся из cbuffer b3,
//   worldPos интерполируется из VS (TEXCOORD0) — так блик корректен на любом меше.
//
// Constant buffers (совпадают с C++ структурами):
//   b2 LightBuffer    — DirectionalLightComponent (RenderSystem.cpp, struct LightBuffer):
//                       float3 LightDir; float LightIntensity; float3 LightColor; float _pad;
//   b3 CameraBuffer   — позиция активной камеры (RenderSystem.cpp, struct CameraBuffer):
//                       float3 CameraPos; float _pad;
//   b4 MaterialBuffer — PhongMaterialComponent (RenderSystem.cpp, struct MaterialBuffer):
//                       float3 MaterialAmbient; float EncodeSRGB;
//                       float3 MaterialDiffuse; float _pad2;
//                       float3 MaterialSpecular; float MaterialShininess;
//                       float4 BaseColor; float2 UVScale; float2 UVOffset;

#include "Surface.hlsli"

float4 PSMain(PS_IN input) : SV_Target
{
    // Нормализуем интерполированную нормаль (после интерполяции длина != 1).
    float3 N = normalize(input.worldNormal);

    // L — из точки к свету (свет «светит» вдоль LightDir).
    float3 L = normalize(-LightDir);

    // V — из точки к наблюдателю: позиция зрителя в пиксельном шейдере.
    float3 V = normalize(CameraPos - input.worldPos);

    // R — отражение падающего света относительно нормали (модель Фонга).
    float3 R = reflect(-L, N);

    float NdotL = max(dot(N, L), 0.0f);
    float shadow = ShadowFactor(input.worldPos);

    // Specular: Фонг с отражённым вектором. Блик только на освещённой стороне.
    float NdotR = max(dot(R, V), 0.0f);
    float3 specular = MaterialSpecular * LightColor * pow(NdotR, MaterialShininess) * LightIntensity;
    float4 albedo = SampleAlbedo(input);
    if (EncodeSRGB > 0.5f) {
        // Albedo modulates diffuse reflection, not the specular highlight.
        specular *= NdotL > 0.0f ? 1.0f : 0.0f;
        float3 lighting = (MaterialAmbient + MaterialDiffuse * NdotL * shadow) * LightColor * LightIntensity;
        return SurfaceOutput(lighting * albedo.rgb + specular * shadow, albedo.a);
    }
    // Preserve the original vertex-tinted highlights for legacy solid materials.
    float3 legacyLighting = (MaterialAmbient + MaterialDiffuse * NdotL * shadow) * LightColor * LightIntensity;
    return SurfaceOutput(legacyLighting * albedo.rgb + specular * input.color.rgb * shadow, albedo.a);
}
