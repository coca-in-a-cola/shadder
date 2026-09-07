#pragma once

#include <algorithm>

#include "CameraComponent.hpp"

class OrbitalCamera : public CameraComponent {
   public:
	OrbitalCamera(
		GameContext* ctx, float initialDistance, Vector3 target_, float minDistance = 3.0, float maxDistance = 100.0
	);
	virtual ~OrbitalCamera() = default;

	void Update(float deltaTime) override;
	Matrix GetViewMatrix() const override;

	void SetTarget(const Vector3& newTarget);

   private:
	Vector3 target;
	Vector3 camPos;

	float distance = 10.0f;
	float minDistance;
	float maxDistance;
	float yaw = 0.0f;
	float pitch = 0.0f;
	float mouseSensitivity = 0.004f;
	float zoomSpeed = 12.0f;

	bool moveForward = false;
	bool moveBackward = false;
};
