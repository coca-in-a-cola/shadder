#include "TextureLoader.h"
#include <WICTextureLoader.h>
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
struct ComScope {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
};

std::shared_ptr<Texture2D> SolidTexture(ID3D11Device* device, bool error) {
    // Missing images are magenta/black; an absent map is neutral white.
    const unsigned pixels[] = { error ? 0xffff00ffu : 0xffffffffu,
                                error ? 0xff000000u : 0xffffffffu,
                                error ? 0xff000000u : 0xffffffffu,
                                error ? 0xffff00ffu : 0xffffffffu };
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = desc.Height = error ? 2 : 1;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = { pixels, desc.Width * 4, 0 };
    ComPtr<ID3D11Texture2D> texture;
    auto result = std::make_shared<Texture2D>();
    if (FAILED(device->CreateTexture2D(&desc, &data, texture.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, result->view.GetAddressOf())))
        return {};
    result->resource = texture;
    result->width = result->height = desc.Width;
    result->mipLevels = 1;
    return result;
}
}

std::wstring TextureLoader::ResolvePath(const std::wstring& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path candidate(path);
    if (!fs::exists(candidate, ec) && !candidate.is_absolute()) {
        std::vector<wchar_t> executable(32768);
        DWORD size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (size && size < executable.size()) {
            fs::path nearExe = fs::path(std::wstring(executable.data(), size)).parent_path() / candidate;
            if (fs::exists(nearExe, ec)) candidate = nearExe;
        }
    }
    auto absolute = fs::weakly_canonical(fs::absolute(candidate, ec), ec);
    return ec ? candidate.lexically_normal().wstring() : absolute.wstring();
}

std::shared_ptr<Texture2D> TextureLoader::Load(ID3D11Device* device, const std::wstring& path) {
    if (!device) return {};
    using Key = std::pair<ID3D11Device*, std::wstring>;
    static std::map<Key, std::weak_ptr<Texture2D>> cache;
    for (auto it = cache.begin(); it != cache.end();) {
        if (it->second.expired()) it = cache.erase(it);
        else ++it;
    }
    const std::wstring resolved = path.empty() ? L"" : ResolvePath(path);
    const Key key{ device, resolved };
    auto found = cache.find(key);
    if (found != cache.end()) {
        if (auto texture = found->second.lock()) return texture;
    }

    std::shared_ptr<Texture2D> result;
    if (path.empty()) {
        result = SolidTexture(device, false);
    } else {
        // DirectXTK caches its WIC factory. Keep COM initialized for the render
        // thread's lifetime, not just one call (the cached factory needs it).
        thread_local ComScope com;
        HRESULT hr = com.result;
        result = std::make_shared<Texture2D>();
        if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
            ComPtr<ID3D11DeviceContext> context;
            device->GetImmediateContext(context.GetAddressOf());
            hr = DirectX::CreateWICTextureFromFileEx(device, context.Get(), resolved.c_str(),
                0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
                DirectX::WIC_LOADER_FORCE_SRGB,
                result->resource.GetAddressOf(), result->view.GetAddressOf());
        }
        if (FAILED(hr)) {
            std::wcerr << L"[TextureLoader] Failed (0x" << std::hex << static_cast<unsigned>(hr)
                       << std::dec << L"): " << resolved << L"; using error texture\n";
            // Do not cache failed paths: SetMaterial() can retry a repaired file.
            return SolidTexture(device, true);
        } else {
            ComPtr<ID3D11Texture2D> texture;
            if (FAILED(result->resource.As(&texture))) return {};
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            result->width = desc.Width;
            result->height = desc.Height;
            result->mipLevels = desc.MipLevels;
            std::wcout << L"[TextureLoader] " << resolved << L" (" << desc.Width << L"x"
                       << desc.Height << L", " << desc.MipLevels << L" mips)\n";
        }
    }
    if (result) cache[key] = result;
    return result;
}
