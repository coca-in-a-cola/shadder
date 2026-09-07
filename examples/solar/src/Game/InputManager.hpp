#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

enum class InputState { Pressed, Released, Held };

struct InputAction {
	std::string name;
	std::vector<SDL_Keycode> keys;
	std::vector<Uint32> mouseButtons;
	InputState state = InputState::Pressed;
	std::function<void()> callback;
};

class InputManager {
   public:
	InputManager();
	~InputManager() = default;

	void ProcessEvent(const SDL_Event& event);
	void Update();

	void RegisterAction(
		const std::string& actionName, const std::vector<SDL_Keycode>& keys = {},
		const std::vector<Uint32>& mouseButtons = {}, InputState state = InputState::Pressed,
		std::function<void()> callback = nullptr
	);

	bool IsActionActive(const std::string& actionName) const;
	bool IsActionPressed(const std::string& actionName) const;
	bool IsActionReleased(const std::string& actionName) const;

	// Mouse
	void GetMouseDelta(float& deltaX, float& deltaY);
	void GetMousePosition(float& x, float& y);

	void EndFrame();

   private:
	std::unordered_map<std::string, InputAction> actions;
	std::unordered_map<SDL_Keycode, bool> keyStates;
	std::unordered_map<SDL_Keycode, bool> keyPressed;
	std::unordered_map<SDL_Keycode, bool> keyReleased;
	std::unordered_map<Uint32, bool> mouseButtonStates;
	std::unordered_map<Uint32, bool> mouseButtonPressed;
	std::unordered_map<Uint32, bool> mouseButtonReleased;

	float mouseX = 0, mouseY = 0;
	float mouseDeltaX = 0, mouseDeltaY = 0;
	bool mouseCaptured = false;
};
