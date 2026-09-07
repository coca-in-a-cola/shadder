#include <SDL3/SDL_main.h>
#include <iostream>

#include "Game/Engine.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
	Engine engine;

	if (!engine.Initialize("Desolation Engine", 1280, 720)) {
		std::cerr << "Engine initialization failed!" << std::endl;
		return -1;
	}

	engine.Run();
	engine.Shutdown();

	return 0;
}
