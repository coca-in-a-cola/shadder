// Launcher: визуальное демо фреймворка — ImGui-меню для сборки и запуска
// примеров из examples/. Каждый пример остаётся самостоятельным проектом.

#include <windows.h>
#include <iostream>

#include "dev/display/DisplayWin32.h"
#include "framework/game/Game.h"
#include "framework/modules/camera/CameraSystem.h"
#include "framework/modules/render/RenderSystem.h"

#include "shared/prefabs/CameraPrefabs.h"

#include "LauncherUI.h"

// Global game pointer for the window procedure
Game* g_Game = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    if (g_Game && g_Game->MessageHandler(hwnd, umessage, wparam, lparam))
    {
        return 0;
    }

    switch (umessage)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, umessage, wparam, lparam);
    }
}

int main()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Create and initialize the Game
    Game game;
    g_Game = &game;

    if (!game.SetDisplay(std::make_unique<DisplayWin32>(L"LABY Launcher", hInstance, WndProc))
             .SetScreenSize({820, 540})
             .Initialize())
    {
        std::cout << "Failed to initialize the game engine!" << '\n';
        return 1;
    }

    World& world = game.GetWorld();
    const ScreenSize screenSize = game.GetScreenSize();

    // Пустая сцена: только камера — лаунчер рисует чистый backbuffer + ImGui.
    OrthoCameraPrefab(static_cast<float>(screenSize.width),
                      static_cast<float>(screenSize.height)).Instantiate(world);

    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    world.RegisterSystem<LauncherUI>(SystemPhase::UPDATE, &game);
    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    std::cout << "[Launcher] LABY demo launcher started. ESC - exit.\n";

    // Run the game loop
    game.Run();

    // ~LauncherUI останавливает дочерний процесс примера и сборку.
    g_Game = nullptr;
    return 0;
}
