// Depth-only pass for Vertex3D. LightViewProj is transposed by C++ like other matrices.
cbuffer WorldBuffer : register(b0)
{
    float4x4 World;
};

cbuffer ShadowMatrixBuffer : register(b5)
{
    float4x4 LightViewProj;
};

struct VS_IN
{
    float4 pos : POSITION0;
};

float4 VSMain(VS_IN input) : SV_POSITION
{
    return mul(mul(input.pos, World), LightViewProj);
}
