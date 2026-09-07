#include "OrbitalCamera.hpp"

OrbitalCamera::OrbitalCamera(
	GameContext* ctx, float initialDistance, Vector3 target_, float minDistance, float maxDistance
)
	: CameraComponent(ctx),
	  target(target_),
	  distance(initialDistance),
	  minDistance(minDistance),
	  maxDistance(maxDistance)
{
	position = Vector3(0, 0, -distance);

	auto* input = ctx->GetInputManager();
	input->RegisterAction("orbital_cam_forward", {SDLK_W}, {}, InputState::Held, [this]() { moveForward = true; });
	input->RegisterAction("orbital_cam_backward", {SDLK_S}, {}, InputState::Held, [this]() { moveBackward = true; });
}

void OrbitalCamera::Update(float deltaTime)
{
	float deltaX, deltaY;
	gameContext->GetInputManager()->GetMouseDelta(deltaX, deltaY);

	if (deltaX != 0.0f || deltaY != 0.0f) {
		yaw -= deltaX * mouseSensitivity;
		pitch += deltaY * mouseSensitivity;
		const float pitchLimit = 1.45f;
		pitch = std::clamp(pitch, -pitchLimit, pitchLimit);
	}

	// Zoom to target
	if (moveForward) {
		distance = std::max(minDistance, distance - zoomSpeed * deltaTime);
	}
	if (moveBackward) {
		distance = std::min(maxDistance, distance + zoomSpeed * deltaTime);
	}

	position = Vector3(0, 0, -distance);

	Matrix RotMat = Matrix::CreateFromYawPitchRoll(yaw, pitch, 0.0);  // // Обновление позиции относительно цели
	camPos = Vector3::Transform(position, RotMat) + target;

	moveForward = moveBackward = false;
}

Matrix OrbitalCamera::GetViewMatrix() const
{
	return Matrix::CreateLookAt(camPos, target, Vector3::Up);
}

void OrbitalCamera::SetTarget(const Vector3& newTarget)
{
	target = newTarget;
}
