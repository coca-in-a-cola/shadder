#pragma once

#include <memory>
#include <vector>

#include "GameContext.hpp"
#include "Mesh.hpp"

class GameComponent {
   public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	float speed = 0.0;
	float angularSpeed = 0.0;

	bool isActive = true;

	GameComponent(GameContext* ctx);
	virtual ~GameComponent() = default;

	void SetVelocity(const Vector3& vel);
	void SetAngularVelocity(const Vector3& angVel);

	void AddChild(std::shared_ptr<GameComponent> child);
	void RemoveChild(GameComponent* child);
	GameComponent* GetParent() const;
	const std::vector<std::shared_ptr<GameComponent>>& GetChildren() const;

	void SetMesh(std::unique_ptr<Mesh>&& m);
	Mesh* GetMesh() const;

	Matrix GetWorldMatrix() const;

	virtual void Draw();
	virtual void Update(float deltaTime);

   protected:
	GameContext* gameContext;
	std::unique_ptr<Mesh> mesh;

	Vector3 velocity = Vector3(0, 0, 0);
	Quaternion rotation = Quaternion::Identity;
	Vector3 angularVelocity = Vector3(0, 0, 0);

	GameComponent* parent = nullptr;
	std::vector<std::shared_ptr<GameComponent>> children;
};
