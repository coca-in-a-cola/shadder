#pragma once

#include "GameComponent.hpp"

class CameraComponent : public GameComponent {
   public:
	CameraComponent(GameContext* ctx);
	virtual ~CameraComponent() = default;

	void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);
	void GetPerspective(float* fovDegrees, float* aspectRatio, float* nearPlane, float* farPlane) const;
	void SetFOV(float fovDegrees);
	void SetAspectRatio(float aspectRatio);

	virtual Matrix GetViewMatrix() const;
	Matrix GetProjectionMatrix() const;

   private:
	float fovRadians = DirectX::XMConvertToRadians(60.0);
	float aspectRatio = 16.0 / 9.0;
	float nearPlane = 0.1;
	float farPlane = 1000.0;
};
