// PixelShader.hlsl — Phong pixel shader (Blinn-Phong)
// Computes: ambient + diffuse * max(dot(N, L), 0) + specular * pow(max(dot(N, H), 0), shininess)
// Light: directional from LightBuffer
// Material params: per-object in constant buffer b4 (MaterialBuffer)
// V = viewDir (from VS), N = worldNormal, L = -LightDir, H = normalize(V + L)

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
    float4 pos        : SV_POSITION;
    float3 worldPos   : TEXCOORD0;
    float3 worldNormal: TEXCOORD1;
    float4 color      : COLOR0;
    float3 viewDir    : TEXCOORD2;
};

float4 PSMain(PS_IN input) : SV_Target
{
    // Normalize interpolated normal
    float3 N = normalize(input.worldNormal);
    
    // Light direction (from surface to light)
    float3 L = normalize(-LightDir);
    
    // View direction (already normalized in VS)
    float3 V = normalize(input.viewDir);
    
    // Half vector for Blinn-Phong
    float3 H = normalize(V + L);
    
    // Diffuse term: Lambert
    float NdotL = max(dot(N, L), 0.0f);
    float3 diffuse = MaterialDiffuse * LightColor * NdotL * LightIntensity;
    
    // Specular term: Blinn-Phong
    float NdotH = max(dot(N, H), 0.0f);
    float specularFactor = pow(NdotH, MaterialShininess);
    float3 specular = MaterialSpecular * LightColor * specularFactor * LightIntensity;
    
    // Ambient term
    float3 ambient = MaterialAmbient * LightColor * LightIntensity;
    
    // Final color (modulate by vertex color)
    float3 finalColor = (ambient + diffuse + specular) * input.color.rgb;
    
    return float4(finalColor, input.color.a);
}