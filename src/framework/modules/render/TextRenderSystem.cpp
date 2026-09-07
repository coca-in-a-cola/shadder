#include "framework/modules/render/TextRenderSystem.h"
#include "framework/modules/render/Font8x8.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/game/Game.h"
#include "core/ecs/Query.h"
#include <d3dcompiler.h>
#include <wrl.h>
#include <vector>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace shadder {

// Shader for text rendering - uses font texture atlas
const char* kTextShaderHLSL = R"(
// TextRender.hlsl - Font rendering shader
// Vertex format: pos (float4), uv (float2), color (float4)

cbuffer WorldBuffer : register(b0)
{
    float4x4 World;
};

cbuffer VPBuffer : register(b1)
{
    float4x4 ViewProj;
};

Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

struct VS_IN
{
    float4 pos : POSITION0;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    float4 worldPos = mul(input.pos, World);
    output.pos = mul(worldPos, ViewProj);
    output.uv = input.uv;
    output.col = input.col;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 texColor = FontTexture.Sample(FontSampler, input.uv);
    // Font texture is grayscale (alpha), multiply by vertex color
    return float4(input.col.rgb, input.col.a * texColor.r);
}
)";

void TextRenderSystem::CreateFontTexture(ID3D11Device* device) {
    // Create a 128x128 texture atlas (16 chars x 12 rows = 192 chars, we need 96)
    // Each char is 8x8, so 16*8=128 wide, 8*12=96 high (we'll use 128x128 for power of 2)
    const uint32_t atlasWidth = 128;
    const uint32_t atlasHeight = 128;
    const uint32_t charsPerRow = 16; // 128/8 = 16
    const uint32_t bytesPerPixel = 4; // RGBA

    std::vector<uint8_t> atlasData(atlasWidth * atlasHeight * bytesPerPixel, 0);

    // Fill atlas with font bitmap data
    for (uint32_t charIdx = 0; charIdx < kFontCharCount; ++charIdx) {
        uint32_t col = charIdx % charsPerRow;
        uint32_t row = charIdx / charsPerRow;
        uint32_t baseX = col * kFontWidth;
        uint32_t baseY = row * kFontHeight;

        for (uint32_t y = 0; y < kFontHeight; ++y) {
            uint8_t bitmapRow = kFont8x8Bitmap[charIdx][y];
            for (uint32_t x = 0; x < kFontWidth; ++x) {
                // MSB = leftmost pixel
                bool pixelOn = (bitmapRow & (0x80 >> x)) != 0;
                uint32_t atlasX = baseX + x;
                uint32_t atlasY = baseY + y;
                uint32_t idx = (atlasY * atlasWidth + atlasX) * bytesPerPixel;
                // White pixel with alpha from font
                uint8_t alpha = pixelOn ? 255 : 0;
                atlasData[idx + 0] = 255; // R
                atlasData[idx + 1] = 255; // G
                atlasData[idx + 2] = 255; // B
                atlasData[idx + 3] = alpha; // A
            }
        }
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = atlasWidth;
    texDesc.Height = atlasHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA texData = {};
    texData.pSysMem = atlasData.data();
    texData.SysMemPitch = atlasWidth * bytesPerPixel;

    device->CreateTexture2D(&texDesc, &texData, fontTexture_.GetAddressOf());

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(fontTexture_.Get(), &srvDesc, fontSRV_.GetAddressOf());
}

void TextRenderSystem::CreateShaders(ID3D11Device* device) {
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    HRESULT hr = D3DCompile(kTextShaderHLSL, strlen(kTextShaderHLSL), nullptr, nullptr, nullptr,
                            "VSMain", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                            vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((const char*)errBlob->GetBufferPointer());
        return;
    }

    hr = D3DCompile(kTextShaderHLSL, strlen(kTextShaderHLSL), nullptr, nullptr, nullptr,
                      "PSMain", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                      psBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((const char*)errBlob->GetBufferPointer());
        return;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader_.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader_.GetAddressOf());

    // Input layout: POSITION0 (float4), TEXCOORD0 (float2), COLOR0 (float4)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,         0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout_.GetAddressOf());
}

void TextRenderSystem::CreateStates(ID3D11Device* device) {
    // Sampler state - point filtering for pixel-perfect font
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sampDesc, samplerState_.GetAddressOf());

    // Blend state - alpha blend
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, blendState_.GetAddressOf());

    // Rasterizer state
    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rastDesc, rasterizerState_.GetAddressOf());

    // Depth stencil state - disable depth test for UI text
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.StencilEnable = FALSE;
    device->CreateDepthStencilState(&dsDesc, depthStencilState_.GetAddressOf());
}

void TextRenderSystem::EnsureResources(ID3D11Device* device) {
    if (resourcesCreated_) return;
    CreateFontTexture(device);
    CreateShaders(device);
    CreateStates(device);
    resourcesCreated_ = true;
}

void TextRenderSystem::BuildTextQuads(World& world, Entity entity, TextComponent& text, Transform3D& transform,
                                      std::vector<TextVertex>& outVertices) {
    if (text.text.empty()) return;

    // Calculate text dimensions
    float charWidth = text.fontSize * text.charSpacing;
    float lineHeight = text.fontSize * text.lineSpacing;

    // Count lines and find max line width
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text.text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);

    float maxLineWidth = 0.0f;
    for (const auto& line : lines) {
        float lineWidth = line.length() * charWidth;
        maxLineWidth = std::max(maxLineWidth, lineWidth);
    }

    // Calculate starting position based on alignment
    float startX = -maxLineWidth * text.hAlign;
    float startY = (lines.size() - 1) * lineHeight * 0.5f - (lines.size() - 1) * lineHeight * text.vAlign;

    // UV coordinates for font atlas
    const float atlasWidth = 128.0f;
    const float atlasHeight = 128.0f;
    const float charUWidth = 8.0f / atlasWidth;
    const float charVHeight = 8.0f / atlasHeight;
    const uint32_t charsPerRow = 16;

    XMFLOAT3 pos = transform.position;
    XMFLOAT3 scale = transform.scale;

    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const std::string& line = lines[lineIdx];
        float lineWidth = line.length() * charWidth;
        float lineStartX = startX - lineWidth * text.hAlign; // Adjust for per-line alignment

        for (size_t charIdx = 0; charIdx < line.length(); ++charIdx) {
            char c = line[charIdx];
            if (c < kFontFirstChar || c > kFontLastChar) continue;

            uint32_t fontIdx = c - kFontFirstChar;
            uint32_t atlasCol = fontIdx % charsPerRow;
            uint32_t atlasRow = fontIdx / charsPerRow;

            float u0 = atlasCol * charUWidth;
            float v0 = atlasRow * charVHeight;
            float u1 = u0 + charUWidth;
            float v1 = v0 + charVHeight;

            float x0 = (lineStartX + charIdx * charWidth) * scale.x;
            float x1 = x0 + charWidth * scale.x;
            float y0 = (startY - lineIdx * lineHeight) * scale.y;
            float y1 = y0 - text.fontSize * scale.y;

            // Two triangles (6 vertices) for the quad
            // Triangle 1: bottom-left, bottom-right, top-right
            outVertices.push_back({ { x0, y1, 0.0f, 1.0f }, { u0, v1 }, text.color });
            outVertices.push_back({ { x1, y1, 0.0f, 1.0f }, { u1, v1 }, text.color });
            outVertices.push_back({ { x1, y0, 0.0f, 1.0f }, { u1, v0 }, text.color });
            // Triangle 2: top-right, top-left, bottom-left
            outVertices.push_back({ { x1, y0, 0.0f, 1.0f }, { u1, v0 }, text.color });
            outVertices.push_back({ { x0, y0, 0.0f, 1.0f }, { u0, v0 }, text.color });
            outVertices.push_back({ { x0, y1, 0.0f, 1.0f }, { u0, v1 }, text.color });
        }
    }
}

void TextRenderSystem::OnUpdate(World& world, float deltaTime) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    if (!ctx || !device) return;

    EnsureResources(device);
    if (!resourcesCreated_) return;

    // Set up pipeline state for text rendering
    ctx->IASetInputLayout(inputLayout_.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vertexShader_.Get(), nullptr, 0);
    ctx->PSSetShader(pixelShader_.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 1, fontSRV_.GetAddressOf());
    ctx->PSSetSamplers(0, 1, samplerState_.GetAddressOf());
    ctx->RSSetState(rasterizerState_.Get());
    ctx->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(depthStencilState_.Get(), 0);

    // Slot b1 (ViewProjection) is set by CameraSystem in PRE_RENDER

    // Query all entities with TextComponent + Transform3D
    Query<TextComponent, Transform3D> q(world);
    q.ForEach([&](Entity entity, TextComponent& text, Transform3D& transform) {
        if (text.text.empty()) return;

        // Build vertex data
        std::vector<TextVertex> vertices;
        BuildTextQuads(world, entity, text, transform, vertices);
        if (vertices.empty()) return;

        // Create vertex buffer
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(TextVertex));
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vbData = { vertices.data(), 0, 0 };
        ComPtr<ID3D11Buffer> vertexBuffer;
        HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, vertexBuffer.GetAddressOf());
        if (FAILED(hr)) return;

        // Set vertex buffer
        UINT stride = sizeof(TextVertex);
        UINT offset = 0;
        ctx->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

        // Set world matrix (identity for text, transform is baked into vertices)
        XMMATRIX world = XMMatrixIdentity();
        XMMATRIX worldT = XMMatrixTranspose(world);

        ComPtr<ID3D11Buffer> worldCB;
        D3D11_BUFFER_DESC cbDesc = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA cbData = { &worldT };
        device->CreateBuffer(&cbDesc, &cbData, worldCB.GetAddressOf());
        ctx->VSSetConstantBuffers(0, 1, worldCB.GetAddressOf());

        // Draw
        ctx->Draw(static_cast<UINT>(vertices.size()), 0);
    });
}

} // namespace shadder