#include "CameraComponent.hpp"

CameraComponent::CameraComponent(GameContext* ctx) : GameComponent(ctx) {}

void CameraComponent::SetPerspective(float fovDegrees_, float aspectRatio_, float nearPlane_, float farPlane_)
{
	fovRadians = DirectX::XMConvertToRadians(fovDegrees_);
	aspectRatio = aspectRatio_;
	nearPlane = nearPlane_;
	farPlane = farPlane_;
}

void CameraComponent::GetPerspective(float* fovDegrees_, float* aspectRatio_, float* nearPlane_, float* farPlane_) const
{
	if (fovDegrees_) {
		*fovDegrees_ = DirectX::XMConvertToDegrees(fovRadians);
	}
	if (aspectRatio_) {
		*aspectRatio_ = aspectRatio;
	}
	if (nearPlane_) {
		*nearPlane_ = nearPlane;
	}
	if (farPlane_) {
		*farPlane_ = farPlane;
	}
}

void CameraComponent::SetFOV(float fovDegrees)
{
	this->fovRadians = DirectX::XMConvertToRadians(fovDegrees);
}

void CameraComponent::SetAspectRatio(float aspectRatio_)
{
	aspectRatio = aspectRatio_;
}

Matrix CameraComponent::GetViewMatrix() const
{
	Vector3 target = position + Vector3::Transform(Vector3(0, 0, 1), rotation);
	Vector3 up = Vector3::Transform(Vector3(0, 1, 0), rotation);

	return Matrix::CreateLookAt(position, target, up);
}

Matrix CameraComponent::GetProjectionMatrix() const
{
	return Matrix::CreatePerspectiveFieldOfView(fovRadians, aspectRatio, nearPlane, farPlane);
}
