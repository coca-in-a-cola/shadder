// =============================================================================
//  RainInstanced.hlsl
//  Один шейдер на ВЕСЬ дождь из треугольников.
//  Геометрия (один треугольник) рисуется через DrawIndexedInstanced,
//  per-instance мировая матрица + цвет приходят из instance-буфера (slot 1).
// =============================================================================

cbuffer VPBuffer : register(b1)
{
    row_major float4x4 ViewProj;
};

struct VS_IN
{
    // Per-vertex (slot 0)
    float4 pos   : POSITION0;
    float4 col   : COLOR0;

    // Per-instance (slot 1): мировая матрица как 4 строки + цвет
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
    float4 icol   : ICOLOR0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);

    float4 worldPos = mul(input.pos, world);
    output.pos = mul(worldPos, ViewProj);
    output.col = input.col * input.icol;

    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    return input.col;
}
