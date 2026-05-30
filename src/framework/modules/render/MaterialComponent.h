#pragma once

#include "ecs/Component.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <string>

struct MaterialComponent : public ComponentBase {
public:
    // --- Описание (CPU, заполняет пользователь) ---------------------------
    // ResourceLoader::UploadAll() компилирует шейдер и создаёт layout/rasterizer.
    enum class LayoutPreset { POS_COLOR };

    // Пусто => используется дефолтный шейдер фреймворка (data/Default2D.hlsl).
    std::wstring shaderPath;
    LayoutPreset layoutPreset = LayoutPreset::POS_COLOR;
    bool uploaded = false; // выставляет ResourceLoader

    // --- GPU-ресурсы (заполняет ResourceLoader) ---------------------------
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    // future: texture, constant buffer
};
