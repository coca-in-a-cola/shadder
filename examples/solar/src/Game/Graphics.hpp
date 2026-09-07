#pragma once

#include <SDL3/SDL.h>
#include <SimpleMath.h>
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class Graphics {
   public:
	Graphics();
	~Graphics();

	bool Initialize(SDL_Window* window, int width, int height);
	void Resize(int width, int height);
	void BeginFrame(const DirectX::SimpleMath::Color& clearColor);
	void EndFrame();
	void SetViewport(int width, int height);

	ID3D11Device* GetDevice() const { return device.Get(); }

	ID3D11DeviceContext* GetContext() const { return context.Get(); }

	int GetWidth() const { return width; }

	int GetHeight() const { return height; }

   private:
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<IDXGISwapChain1> swapChain;
	ComPtr<ID3D11RenderTargetView> renderTargetView;
	ComPtr<ID3D11DepthStencilView> depthStencilView;
	ComPtr<ID3D11Texture2D> depthStencilBuffer;

	int width = 0;
	int height = 0;
};
