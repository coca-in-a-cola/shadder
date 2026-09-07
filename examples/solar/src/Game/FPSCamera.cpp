#include "FPSCamera.hpp"

FPSCamera::FPSCamera(GameContext* ctx) : CameraComponent(ctx)
{
	auto* input = ctx->GetInputManager();

	// Регистрация действий (коллбеки устанавливают флаги)
	input->RegisterAction("cam_forward", {SDLK_W}, {}, InputState::Held, [this]() { moveForward = true; });
	input->RegisterAction("cam_backward", {SDLK_S}, {}, InputState::Held, [this]() { moveBackward = true; });
	input->RegisterAction("cam_left", {SDLK_A}, {}, InputState::Held, [this]() { moveLeft = true; });
	input->RegisterAction("cam_right", {SDLK_D}, {}, InputState::Held, [this]() { moveRight = true; });

	speed = 15.0;
}

void FPSCamera::Update(float deltaTime)
{
	// Вращение мышью
	float deltaX, deltaY;
	gameContext->GetInputManager()->GetMouseDelta(deltaX, deltaY);

	if (deltaX != 0.0f || deltaY != 0.0f) {
		yaw -= deltaX * mouseSensitivity;
		pitch += deltaY * mouseSensitivity;

		// Ограничение pitch (по высоте, чтобы не перевернуться)
		const float limit = 1.55f;
		pitch = std::max(-limit, std::min(limit, pitch));

		rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0);
	}

	// Вычисляем направления относительно взгляда
	Vector3 right = Vector3::Transform(Vector3(-1, 0, 0), Matrix::CreateRotationY(yaw));
	Vector3 worldForward = Vector3::Transform(Vector3(0, 0, 1), rotation);

	// Движение относительно направления взгляда
	Vector3 moveDir(0, 0, 0);
	if (moveForward) {
		moveDir += worldForward;
	}
	if (moveBackward) {
		moveDir -= worldForward;
	}
	if (moveRight) {
		moveDir += right;
	}
	if (moveLeft) {
		moveDir -= right;
	}

	if (moveDir.LengthSquared() > 0) {
		moveDir.Normalize();
		position += moveDir * speed * deltaTime;
	}

	// Сброс флагов после использования
	moveForward = moveBackward = moveLeft = moveRight = false;
}
