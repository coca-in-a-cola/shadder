// VertexShader.hlsl — Phong vertex shader
// Input: POSITION (float4), NORMAL (float3), COLOR (float4)
// Output to PS: worldPos (float3), worldNormal (float3), color (float4), viewDir (float3)
// Constant buffers: b0 = World, b1 = ViewProj, b2 = Light (direction, color, intensity), b3 = CameraPos

cbuffer WorldBuffer : register(b0)
{
    float4x4 World;
};

cbuffer VPBuffer : register(b1)
{
    float4x4 ViewProj;
};

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

struct VS_IN
{
    float4 pos    : POSITION0;
    float3 normal : NORMAL0;
    float4 color  : COLOR0;
};

struct PS_IN
{
    float4 pos        : SV_POSITION;
    float3 worldPos   : TEXCOORD0;
    float3 worldNormal: TEXCOORD1;
    float4 color      : COLOR0;
    float3 viewDir    : TEXCOORD2;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    
    // World position
    float4 worldPos4 = mul(input.pos, World);
    output.worldPos = worldPos4.xyz;
    
    // World normal (normal matrix = inverse transpose of upper 3x3 of World)
    // For uniform scale + rotation only, just rotate by upper 3x3
    float3x3 world3x3 = (float3x3)World;
    output.worldNormal = normalize(mul(input.normal, world3x3));
    
    // View direction (camera to fragment)
    output.viewDir = normalize(CameraPos - output.worldPos);
    
    // Clip position
    output.pos = mul(worldPos4, ViewProj);
    
    // Pass through vertex color
    output.color = input.color;
    
    return output;
}