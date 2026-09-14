#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <string>

struct Texture2D {
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    unsigned width = 0;
    unsigned height = 0;
    unsigned mipLevels = 0;
};

// Render-thread only. Materials own resources; the device-keyed weak cache never
// keeps a device or an unused texture alive. Empty path selects a white texture.
class TextureLoader {
public:
    static std::shared_ptr<Texture2D> Load(ID3D11Device* device,
                                         const std::wstring& path);
    static std::wstring ResolvePath(const std::wstring& path);
};
