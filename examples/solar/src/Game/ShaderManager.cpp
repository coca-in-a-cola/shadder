#include "ShaderManager.hpp"

ShaderManager::ShaderManager(GameContext* ctx) : gameContext(ctx)
{
	ComPtr<ID3DBlob> vsBlobPtr, psBlobPtr, errorBlob;
	ID3D11Device* device = gameContext->GetGraphicsDevice();

	// Компиляция вершинного шейдера
	HRESULT hr = D3DCompileFromFile(
		L"shaders/VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlobPtr, &errorBlob
	);
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		throw std::runtime_error("Failed to compile vertex shader");
	}

	// Компиляция пиксельного шейдера
	hr = D3DCompileFromFile(
		L"shaders/PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlobPtr, &errorBlob
	);
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		throw std::runtime_error("Failed to compile pixel shader");
	}

	// Создание шейдеров
	device->CreateVertexShader(vsBlobPtr->GetBufferPointer(), vsBlobPtr->GetBufferSize(), nullptr, &vertexShader);
	device->CreatePixelShader(psBlobPtr->GetBufferPointer(), psBlobPtr->GetBufferSize(), nullptr, &pixelShader);

	// Input Layout
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	device->CreateInputLayout(layout, 2, vsBlobPtr->GetBufferPointer(), vsBlobPtr->GetBufferSize(), &inputLayout);

	// Constant Buffer
	D3D11_BUFFER_DESC cbd = {};
	cbd.ByteWidth = sizeof(Matrix) * 3;	 // world, view, projection
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	device->CreateBuffer(&cbd, nullptr, &constantBuffer);
}

void ShaderManager::Apply()
{
	auto context = gameContext->GetGraphicsContext();

	context->VSSetShader(vertexShader.Get(), nullptr, 0);
	context->PSSetShader(pixelShader.Get(), nullptr, 0);
	context->IASetInputLayout(inputLayout.Get());
	context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
}

void ShaderManager::UpdateConstants(const void* data)
{
	auto device = gameContext->GetGraphicsContext();
	device->UpdateSubresource(constantBuffer.Get(), 0, nullptr, data, 0, 0);
}
