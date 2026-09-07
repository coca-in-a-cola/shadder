#include "Mesh.hpp"

Mesh::Mesh() = default;
Mesh::~Mesh() = default;

bool Mesh::CreateCube(GameContext* context)
{
	auto device = context->GetGraphicsDevice();

	vertices.clear();
	indices.clear();

	// Вершины куба (8 точек)
	vertices = {
		{Vector3(-0.5f, -0.5f, -0.5f), Colors::Red},	// 0 красный
		{Vector3(-0.5f, 0.5f, -0.5f), Colors::Green},	// 1 зеленый
		{Vector3(0.5f, 0.5f, -0.5f), Colors::Blue},		// 2 синий
		{Vector3(0.5f, -0.5f, -0.5f), Colors::Yellow},	// 3 желтый
		{Vector3(-0.5f, -0.5f, 0.5f), Colors::Purple},	// 4 пурпурный
		{Vector3(-0.5f, 0.5f, 0.5f), Colors::Cyan},		// 5 бирюзовый
		{Vector3(0.5f, 0.5f, 0.5f), Colors::White},		// 6 белый
		{Vector3(0.5f, -0.5f, 0.5f), Colors::Black},	// 7 черный
	};

	// Индексы (36 для 12 треугольников)
	indices = {
		// Передняя грань
		0,
		2,
		1,
		0,
		3,
		2,
		// Задняя грань
		4,
		5,
		6,
		4,
		6,
		7,
		// Левая грань
		4,
		1,
		5,
		4,
		0,
		1,
		// Правая грань
		3,
		6,
		2,
		3,
		7,
		6,
		// Верхняя грань
		1,
		6,
		5,
		1,
		2,
		6,
		// Нижняя грань
		4,
		3,
		0,
		4,
		7,
		3,
	};

	// Создание буфера вершин
	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(Vertex) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices.data();
	if (FAILED(device->CreateBuffer(&vbd, &initData, &vertexBuffer))) {
		return false;
	}

	// Создание буфера индексов
	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(WORD) * indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	initData.pSysMem = indices.data();
	if (FAILED(device->CreateBuffer(&ibd, &initData, &indexBuffer))) {
		return false;
	}

	return true;
}

bool Mesh::CreateSphere(GameContext* context, int slices, int stacks)
{
	auto device = context->GetGraphicsDevice();

	vertices.clear();
	indices.clear();

	// Верхний полюс
	vertices.push_back({Vector3(0.0f, 0.5f, 0.0f), Colors::White});

	// Кольца шара
	for (int stack = 1; stack < stacks; ++stack) {
		float phi = DirectX::XM_PI * stack / stacks;
		float y = 0.5f * cosf(phi);
		float r = 0.5f * sinf(phi);

		for (int slice = 0; slice <= slices; ++slice) {
			float theta = 2.0f * DirectX::XM_PI * slice / slices;
			float x = r * cosf(theta);
			float z = r * sinf(theta);

			// Градиент цвета от синего к красному
			float t = static_cast<float>(stack) / stacks;
			Vector4 color = Vector4::Lerp(Colors::Blue, Colors::Red, t);

			vertices.push_back({Vector3(x, y, z), color});
		}
	}

	// Нижний полюс
	vertices.push_back({Vector3(0.0f, -0.5f, 0.0f), Colors::Black});

	// Индексы для верхнего полюса (треугольники веером)
	for (int slice = 0; slice < slices; ++slice) {
		indices.push_back(0);
		indices.push_back(slice + 1);
		indices.push_back(slice + 2);
	}

	// Индексы для колец
	for (int stack = 0; stack < stacks - 2; ++stack) {
		int currentRow = 1 + stack * (slices + 1);
		int nextRow = currentRow + slices + 1;

		for (int slice = 0; slice < slices; ++slice) {
			indices.push_back(currentRow + slice);
			indices.push_back(nextRow + slice);
			indices.push_back(currentRow + slice + 1);

			indices.push_back(currentRow + slice + 1);
			indices.push_back(nextRow + slice);
			indices.push_back(nextRow + slice + 1);
		}
	}

	// Индексы для нижнего полюса
	int lastVertex = static_cast<int>(vertices.size()) - 1;
	int lastRowStart = lastVertex - slices - 1;

	for (int slice = 0; slice < slices; ++slice) {
		indices.push_back(lastRowStart + slice);
		indices.push_back(lastVertex);
		indices.push_back(lastRowStart + slice + 1);
	}

	// Создание буферов
	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(Vertex) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices.data();
	if (FAILED(device->CreateBuffer(&vbd, &initData, &vertexBuffer))) {
		return false;
	}

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(WORD) * indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	initData.pSysMem = indices.data();
	if (FAILED(device->CreateBuffer(&ibd, &initData, &indexBuffer))) {
		return false;
	}

	return true;
}

bool Mesh::CreateGrid(GameContext* context, float size, int divisions, const Vector4& color)
{
	if (size <= 0.0f || divisions <= 0) {
		return false;
	}

	auto device = context->GetGraphicsDevice();

	vertices.clear();
	indices.clear();

	float half = size * 0.5f;
	float step = size / static_cast<float>(divisions);

	for (int i = 0; i <= divisions; ++i) {
		float coord = -half + i * step;

		// линия вдоль X
		vertices.push_back({Vector3(-half, 0.0f, coord), color});
		vertices.push_back({Vector3(half, 0.0f, coord), color});

		// линия вдоль Z
		vertices.push_back({Vector3(coord, 0.0f, -half), color});
		vertices.push_back({Vector3(coord, 0.0f, half), color});
	}

	indices.resize(vertices.size());
	for (WORD i = 0; i < static_cast<WORD>(indices.size()); ++i) {
		indices[i] = i;
	}

	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(Vertex) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices.data();
	if (FAILED(device->CreateBuffer(&vbd, &initData, &vertexBuffer))) {
		return false;
	}

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(WORD) * indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	initData.pSysMem = indices.data();
	if (FAILED(device->CreateBuffer(&ibd, &initData, &indexBuffer))) {
		return false;
	}

	topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	return true;
}

void Mesh::SetTopology(D3D11_PRIMITIVE_TOPOLOGY newTopology)
{
	topology = newTopology;
}

void Mesh::Draw(GameContext* ctx)
{
	auto device = ctx->GetGraphicsContext();

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	device->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	device->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	device->IASetPrimitiveTopology(topology);
	device->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);
}
