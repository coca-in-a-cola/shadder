#include "Graphics.hpp"

#include <stdexcept>

Graphics::Graphics() = default;
Graphics::~Graphics() = default;

bool Graphics::Initialize(SDL_Window* window, int w, int h)
{
	width = w;
	height = h;

	// Получаем HWND из SDL3 окна
	SDL_PropertiesID props = SDL_GetWindowProperties(window);
	HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
	if (!hwnd) {
		return false;
	}

	// Создаем устройство и swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
	UINT createDeviceFlags = 0;

#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ComPtr<IDXGIDevice> dxgiDevice;
	ComPtr<IDXGIAdapter> adapter;
	ComPtr<IDXGIFactory2> factory;

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createDeviceFlags,
		featureLevels,
		1,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&context
	);

	if (FAILED(hr)) {
		return false;
	}

	device.As(&dxgiDevice);
	dxgiDevice->GetAdapter(&adapter);
	adapter->GetParent(IID_IDXGIFactory2, &factory);

	factory->CreateSwapChainForHwnd(device.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);

	// Создаем Render Target View
	ComPtr<ID3D11Texture2D> backBuffer;
	swapChain->GetBuffer(0, IID_ID3D11Texture2D, &backBuffer);
	device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);

	// Создаем Depth Stencil Buffer
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);
	device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, &depthStencilView);

	SetViewport(width, height);
	return true;
}

void Graphics::SetViewport(int w, int h)
{
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(w);
	viewport.Height = static_cast<float>(h);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);
}

void Graphics::Resize(int w, int h)
{
	if (width == w && height == h) {
		return;
	}
	width = w;
	height = h;

	context->OMSetRenderTargets(0, nullptr, nullptr);
	renderTargetView.Reset();
	depthStencilView.Reset();
	depthStencilBuffer.Reset();

	swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

	ComPtr<ID3D11Texture2D> backBuffer;
	swapChain->GetBuffer(0, IID_ID3D11Texture2D, &backBuffer);
	device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	device->CreateTexture2D(&depthDesc, nullptr, &depthStencilBuffer);
	device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, &depthStencilView);

	SetViewport(width, height);
}

void Graphics::BeginFrame(const DirectX::SimpleMath::Color& clearColor)
{
	context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
	context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), depthStencilView.Get());
}

void Graphics::EndFrame()
{
	swapChain->Present(1, 0);  // VSync включен
}
