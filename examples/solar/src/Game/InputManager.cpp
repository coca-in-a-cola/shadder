#include "InputManager.hpp"

InputManager::InputManager()
{
	// Инициализация состояний клавиш
}

void InputManager::ProcessEvent(const SDL_Event& event)
{
	switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
			keyStates[event.key.key] = true;
			keyPressed[event.key.key] = true;
			break;
		case SDL_EVENT_KEY_UP:
			keyStates[event.key.key] = false;
			keyReleased[event.key.key] = true;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			mouseButtonStates[event.button.button] = true;
			mouseButtonPressed[event.button.button] = true;
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			mouseButtonStates[event.button.button] = false;
			mouseButtonReleased[event.button.button] = true;
			break;
		case SDL_EVENT_MOUSE_MOTION:
			mouseX = event.motion.x;
			mouseY = event.motion.y;
			mouseDeltaX += event.motion.xrel;
			mouseDeltaY += event.motion.yrel;
			break;
	}
}

void InputManager::Update()
{
	// Выполнение коллбеков для активных действий
	for (auto& pair : actions) {
		InputAction& action = pair.second;
		bool isActive = false;

		// Проверка клавиш
		for (SDL_Keycode key : action.keys) {
			if (action.state == InputState::Pressed && keyPressed[key]) {
				isActive = true;
			} else if (action.state == InputState::Released && keyReleased[key]) {
				isActive = true;
			} else if (action.state == InputState::Held && keyStates[key]) {
				isActive = true;
			}
		}

		// Проверка кнопок мыши
		for (Uint32 button : action.mouseButtons) {
			if (action.state == InputState::Pressed && mouseButtonPressed[button]) {
				isActive = true;
			} else if (action.state == InputState::Released && mouseButtonReleased[button]) {
				isActive = true;
			} else if (action.state == InputState::Held && mouseButtonStates[button]) {
				isActive = true;
			}
		}

		if (isActive && action.callback) {
			action.callback();
		}
	}
}

void InputManager::RegisterAction(
	const std::string& actionName, const std::vector<SDL_Keycode>& keys, const std::vector<Uint32>& mouseButtons,
	InputState state, std::function<void()> callback
)
{
	InputAction action;
	action.name = actionName;
	action.keys = keys;
	action.mouseButtons = mouseButtons;
	action.state = state;
	action.callback = callback;
	actions[actionName] = action;
}

bool InputManager::IsActionActive(const std::string& actionName) const
{
	auto it = actions.find(actionName);
	if (it == actions.end()) {
		return false;
	}

	const InputAction& action = it->second;

	for (SDL_Keycode key : action.keys) {
		if (action.state == InputState::Pressed && keyPressed.at(key)) {
			return true;
		}
		if (action.state == InputState::Released && keyReleased.at(key)) {
			return true;
		}
		if (action.state == InputState::Held && keyStates.at(key)) {
			return true;
		}
	}

	for (Uint32 button : action.mouseButtons) {
		if (action.state == InputState::Pressed && mouseButtonPressed.at(button)) {
			return true;
		}
		if (action.state == InputState::Released && mouseButtonReleased.at(button)) {
			return true;
		}
		if (action.state == InputState::Held && mouseButtonStates.at(button)) {
			return true;
		}
	}

	return false;
}

bool InputManager::IsActionPressed(const std::string& actionName) const
{
	auto it = actions.find(actionName);
	if (it == actions.end()) {
		return false;
	}

	const InputAction& action = it->second;

	for (SDL_Keycode key : action.keys) {
		if (keyPressed.count(key) && keyPressed.at(key)) {
			return true;
		}
	}

	for (Uint32 button : action.mouseButtons) {
		if (mouseButtonPressed.count(button) && mouseButtonPressed.at(button)) {
			return true;
		}
	}

	return false;
}

bool InputManager::IsActionReleased(const std::string& actionName) const
{
	auto it = actions.find(actionName);
	if (it == actions.end()) {
		return false;
	}

	const InputAction& action = it->second;

	for (SDL_Keycode key : action.keys) {
		if (keyReleased.count(key) && keyReleased.at(key)) {
			return true;
		}
	}

	for (Uint32 button : action.mouseButtons) {
		if (mouseButtonReleased.count(button) && mouseButtonReleased.at(button)) {
			return true;
		}
	}

	return false;
}

void InputManager::GetMouseDelta(float& deltaX, float& deltaY)
{
	deltaX = mouseDeltaX;
	deltaY = mouseDeltaY;
}

void InputManager::GetMousePosition(float& x, float& y)
{
	x = mouseX;
	y = mouseY;
}

void InputManager::EndFrame()
{
	keyPressed.clear();
	keyReleased.clear();
	mouseButtonPressed.clear();
	mouseButtonReleased.clear();
	mouseDeltaX = 0;
	mouseDeltaY = 0;
}
