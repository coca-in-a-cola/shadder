#pragma once

#include <SDL3/SDL.h>
#include <deque>
#include <iostream>
#include <memory>
#include <vector>

#include "../GameSet/Planets.hpp"
#include "Colors.hpp"
#include "FPSCamera.hpp"
#include "GameComponent.hpp"
#include "GameContext.hpp"
#include "Graphics.hpp"
#include "InputManager.hpp"
#include "Mesh.hpp"
#include "OrbitalCamera.hpp"
#include "ShaderManager.hpp"

class Engine : public GameContext {
   public:
	Engine() = default;
	virtual ~Engine() = default;

	bool Initialize(const char* title, int width, int height);
	void Run();
	void Shutdown();

	ID3D11DeviceContext* GetGraphicsContext() override;
	ID3D11Device* GetGraphicsDevice() override;
	InputManager* GetInputManager() override;

   private:
	void ProcessEvents();
	void Update(float deltaTime);
	void UpdateComponent(const std::shared_ptr<GameComponent>& component, float deltaTime);
	void Render();
	void DrawComponent(const std::shared_ptr<GameComponent>& component, const Matrix& view, const Matrix& proj);

	SDL_Window* window = nullptr;
	bool running = false;

	std::vector<std::shared_ptr<GameComponent>> gameComponents;

	std::unique_ptr<Graphics> graphics;
	std::shared_ptr<FPSCamera> camera;
	std::shared_ptr<OrbitalCamera> fixedCamera;
	std::shared_ptr<CameraComponent> activeCamera;
	std::unique_ptr<Mesh> cube;
	std::unique_ptr<ShaderManager> shaders;
	std::unique_ptr<InputManager> inputManager;

	int width, height;
};
