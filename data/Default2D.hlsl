// Default2D.hlsl — базовый 2D-шейдер фреймворка.
// Совместим с RenderSystem (World в slot b0) и CameraSystem (ViewProj в slot b1).
// Формат вершины: POSITION0 (float4) + COLOR0 (float4) — пресет layout POS_COLOR.

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
    float4 pos : POSITION0;
    float4 col : COLOR0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    float4 worldPos = mul(input.pos, World);
    output.pos = mul(worldPos, ViewProj);
    output.col = input.col;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    return input.col;
}
