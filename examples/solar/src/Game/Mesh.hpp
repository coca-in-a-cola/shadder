#pragma once

#include <SimpleMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "Colors.hpp"
#include "GameContext.hpp"

using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

struct Vertex {
	Vector3 position;
	Vector4 color;
};

class Mesh {
   public:
	Mesh();
	~Mesh();

	bool CreateCube(GameContext* context);
	bool CreateSphere(GameContext* context, int slices = 16, int stacks = 16);
	bool CreateGrid(
		GameContext* context, float size = 100.0f, int divisions = 20, const Vector4& color = Colors::LightGray
	);
	void SetTopology(D3D11_PRIMITIVE_TOPOLOGY topology);
	void Draw(GameContext* context);

	size_t GetIndexCount() const { return indices.size(); }

   private:
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	std::vector<Vertex> vertices;
	std::vector<WORD> indices;
	D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};
