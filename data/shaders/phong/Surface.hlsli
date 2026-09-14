#ifndef SHADDER_SURFACE_HLSLI
#define SHADDER_SURFACE_HLSLI

Texture2D DiffuseMap : register(t0);
SamplerState DiffuseSampler : register(s0);

cbuffer LightBuffer : register(b2)
{
    float3 LightDir;
    float LightIntensity;
    float3 LightColor;
    float _lightPad;
};

cbuffer CameraBuffer : register(b3)
{
    float3 CameraPos;
    float _cameraPad;
};

// Must match RenderSystem::MaterialBuffer. Keep the original first 48 bytes.
cbuffer MaterialBuffer : register(b4)
{
    float3 MaterialAmbient;
    float EncodeSRGB;
    float3 MaterialDiffuse;
    float _materialPad;
    float3 MaterialSpecular;
    float MaterialShininess;
    float4 BaseColor;
    float2 UVScale;
    float2 UVOffset;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 color : COLOR0;
    float2 uv : TEXCOORD2;
};

float3 LinearToSRGB(float3 value)
{
    value = max(value, 0.0f);
    return lerp(1.055f * pow(value, 1.0f / 2.4f) - 0.055f,
                12.92f * value, value <= 0.0031308f);
}

float4 SampleAlbedo(PS_IN input)
{
    // The SRGB resource view decodes RGB; alpha remains linear.
    return DiffuseMap.Sample(DiffuseSampler, input.uv * UVScale + UVOffset)
           * BaseColor * input.color;
}

float3 DiffuseLighting(float ndotl)
{
    return (MaterialAmbient + MaterialDiffuse * ndotl) * LightColor * LightIntensity;
}

float4 SurfaceOutput(float3 lighting, float alpha)
{
    // The framework uses a UNORM back buffer, so encoding is explicit here.
    // This leaves existing 2D/ImGui passes and legacy solid materials untouched.
    return float4(EncodeSRGB > 0.5f ? LinearToSRGB(lighting) : lighting, alpha);
}
#endif
