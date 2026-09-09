// VertexShader.hlsl — Phong vertex shader.
//
// Передаёт в PS МИРОВЫЕ позицию и нормаль (для модели Фонга) + цвет вершины.
//
// Constant buffers (совпадают с C++ структурами):
//   b0 WorldBuffer  — XMMATRIX (транспонированная world-матрица объекта, RenderSystem.cpp)
//   b1 VPBuffer     — XMMATRIX (транспонированная View*Proj, CameraSystem.cpp)
//   b2 LightBuffer  — DirectionalLightComponent: float3 dir; float intensity; float3 color; float pad;
//   b3 CameraBuffer — float3 CameraPos; float pad; (использовался в VS раньше;
//                     теперь V считается в PS — b3 объявлен и в PixelShader.hlsl)
//
// Вершинный формат: POSITION0 (float4) + NORMAL0 (float3) + COLOR0 (float4)
// — пресет POS_NORMAL_COLOR в ResourceLoader.cpp.

cbuffer WorldBuffer : register(b0)
{
    float4x4 World;
};

cbuffer VPBuffer : register(b1)
{
    float4x4 ViewProj;
};

struct VS_IN
{
    float4 pos    : POSITION0;
    float3 normal : NORMAL0;
    float4 color  : COLOR0;
};

struct PS_IN
{
    float4 pos         : SV_POSITION;
    float3 worldPos    : TEXCOORD0; // мировая позиция — для V = normalize(CameraPos - worldPos) в PS
    float3 worldNormal : TEXCOORD1; // мировая нормаль — для N·L и R·V в PS
    float4 color       : COLOR0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    // Мировая позиция
    float4 worldPos4 = mul(input.pos, World);
    output.worldPos = worldPos4.xyz;

    // Мировая нормаль: для равномерного масштаба + поворота достаточно верхней 3x3.
    // (Нормализуется в PS после интерполяции.)
    float3x3 world3x3 = (float3x3)World;
    output.worldNormal = mul(input.normal, world3x3);

    // Клип-позиция
    output.pos = mul(worldPos4, ViewProj);

    // Цвет вершины насквозь (модулирует материал в PS)
    output.color = input.color;

    return output;
}
