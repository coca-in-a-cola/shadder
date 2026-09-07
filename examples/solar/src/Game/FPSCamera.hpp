#pragma once

#include <algorithm>

#include "CameraComponent.hpp"

class FPSCamera : public CameraComponent {
   public:
	FPSCamera(GameContext* ctx);
	virtual ~FPSCamera() = default;

	void Update(float deltaTime) override;

   private:
	float yaw = 0.0f;
	float pitch = 0.0f;
	float mouseSensitivity = 0.0005f;

	// Флаги движения (обновляются коллбеками InputManager)
	bool moveForward = false;
	bool moveBackward = false;
	bool moveLeft = false;
	bool moveRight = false;
};
