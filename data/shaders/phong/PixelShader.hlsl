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
//                       float3 MaterialAmbient; float _pad1;
//                       float3 MaterialDiffuse; float _pad2;
//                       float3 MaterialSpecular; float MaterialShininess;

cbuffer LightBuffer : register(b2)
{
    float3 LightDir;
    float  LightIntensity;
    float3 LightColor;
    float  _pad1;
};

cbuffer CameraBuffer : register(b3)
{
    float3 CameraPos;
    float  _pad2;
};

cbuffer MaterialBuffer : register(b4)
{
    float3 MaterialAmbient;
    float  _pad3;
    float3 MaterialDiffuse;
    float  _pad4;
    float3 MaterialSpecular;
    float  MaterialShininess;
};

struct PS_IN
{
    float4 pos         : SV_POSITION;
    float3 worldPos    : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 color       : COLOR0;
};

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

    // Ambient: постоянная подсветка.
    float3 ambient = MaterialAmbient * LightColor * LightIntensity;

    // Diffuse: Ламберт.
    float NdotL = max(dot(N, L), 0.0f);
    float3 diffuse = MaterialDiffuse * LightColor * NdotL * LightIntensity;

    // Specular: Фонг с отражённым вектором. Блик только на освещённой стороне.
    float NdotR = max(dot(R, V), 0.0f);
    float3 specular = MaterialSpecular * LightColor * pow(NdotR, MaterialShininess) * LightIntensity;

    float3 finalColor = (ambient + diffuse + specular) * input.color.rgb;

    return float4(finalColor, input.color.a);
}
