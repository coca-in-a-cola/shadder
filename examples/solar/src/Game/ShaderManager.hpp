#pragma once

#include <SimpleMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "GameContext.hpp"

using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

class ShaderManager {
   public:
	struct ShaderBytecode {
		const BYTE* data;
		size_t size;
	};

	ShaderManager(GameContext* ctx);

	void Apply();
	void UpdateConstants(const void* data);

   private:
	bool CompileShaders();

	GameContext* gameContext = nullptr;

	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11InputLayout> inputLayout;
	ComPtr<ID3D11Buffer> constantBuffer;

	std::vector<BYTE> vsBlob;
	std::vector<BYTE> psBlob;
};
