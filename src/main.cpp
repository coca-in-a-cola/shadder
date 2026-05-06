// main.cpp : Entry point — creates window, initializes Game, runs game loop.

#include <windows.h>
#include <iostream>
#include <memory>

#include "core/DisplayWin32.h"
#include "core/game/Game.h"
#include "core/game/TriangleComponent.h"

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

    // Create the display window
    DisplayWin32 display(L"My3DApp", hInstance, 800, 800, WndProc);

    // Create and initialize the Game
    Game game;
    g_Game = &game;

    if (!game.Initialize(&display))
    {
        std::cout << "Failed to initialize the game engine!" << '\n';
        return 1;
    }

    // Create and add the triangle component
    auto triangle = std::make_unique<TriangleComponent>(&game);
    triangle->Initialize();
    game.AddComponent(std::move(triangle));

    // Run the game loop
    game.Run();

    g_Game = nullptr;
    return 0;
}
