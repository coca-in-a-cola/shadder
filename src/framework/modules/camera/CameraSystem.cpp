#include "framework/modules/camera/CameraSystem.h"
#include "dev/display/DisplayWin32.h"
#include "ecs/Query.h"
#include "framework/game/Game.h"
#include "framework/modules/camera/CameraComponent.h"
#include <DirectXMath.h>

// slot 1 collides with old RenderSystem VP (which used the same).
// CameraSystem takes exclusive ownership of this slot.
static constexpr UINT kCameraSlot = 1;

void CameraSystem::OnUpdate(World &world, float) {
		if (!game_) {
				return;
		}
		ID3D11DeviceContext *ctx = game_->GetContext();
		ID3D11Device *device = game_->GetDevice();
		DisplayWin32 *display = game_->GetDisplay();
		if (!ctx || !device || !display) {
				return;
		}

		// Найти первую активную камеру (заглушка: в будущем полноценный CameraSystem
		// будет перебирать все камеры и выбирать priority/main, сейчас — первая).
		CameraComponent *cam = nullptr;
		Query<CameraComponent> q(world);
		q.ForEach([&](Entity, CameraComponent &c) {
				if (c.active && !cam) {
						cam = &c;
				}
		});
		if (!cam) {
				return;
		}

		// Обновить размеры/aspect при ресайзе окна.
		const int w = display->GetWidth();
		const int h = display->GetHeight();
		if (w > 0 && h > 0) {
				cam->aspect = static_cast<float>(w) / static_cast<float>(h);
				cam->screenW = static_cast<float>(w);
				cam->screenH = static_cast<float>(h);
		}

		using namespace DirectX;

		// View * Projection в LH-системе (фреймворк использует DirectXMath).
		XMMATRIX view;
		XMMATRIX proj;
		if (cam->projection == CameraComponent::Projection::ORTHO_SCREEN) {
				// Экранные пиксели: начало координат в левом-верхнем углу, ось Y вниз.
				// off-center ortho: left=0, right=W, bottom=H, top=0 -> y растёт вниз.
				view = XMMatrixIdentity();
				proj = XMMatrixOrthographicOffCenterLH(
								0.0f, cam->screenW, // left, right
								cam->screenH, 0.0f, // bottom, top (перевёрнуты => Y вниз)
								cam->nearZ, cam->farZ);
		} else {
				view = XMMatrixLookAtLH(
								XMLoadFloat3(&cam->eye),
								XMLoadFloat3(&cam->target),
								XMLoadFloat3(&cam->up));
				proj = XMMatrixPerspectiveFovLH(
								cam->fovY, cam->aspect,
								cam->nearZ, cam->farZ);
		}
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);

		// Конвенция: HLSL cbuffer column-major (default) + mul(rowVec, M).
		// CPU транспонирует перед записью: столбцы оригинала → строки в памяти,
		// HLSL читает как столбцы → восстанавливает оригинальную матрицу.
		// mul(v, ViewProj) при этом вычисляет корректный результат.
		XMFLOAT4X4 data;
		XMStoreFloat4x4(&data, XMMatrixTranspose(viewProj));

		if (!cb_) {
				D3D11_BUFFER_DESC bd = {};
				bd.ByteWidth = sizeof(XMFLOAT4X4);
				bd.Usage = D3D11_USAGE_DYNAMIC;
				bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				D3D11_SUBRESOURCE_DATA sd = { &data };
				device->CreateBuffer(&bd, &sd, cb_.GetAddressOf());
		} else {
				// Матрица пересчитывается каждый кадр (камера/окно могут меняться).
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if (SUCCEEDED(ctx->Map(cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
						memcpy(mapped.pData, &data, sizeof(XMFLOAT4X4));
						ctx->Unmap(cb_.Get(), 0);
				}
		}
		width_ = w;
		height_ = h;

		ctx->VSSetConstantBuffers(kCameraSlot, 1, cb_.GetAddressOf());
}
