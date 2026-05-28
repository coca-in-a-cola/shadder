#include "framework/game/Game.h"
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

Game::Game()
    : display(nullptr)
    , totalTime(0.0f)
    , frameCount(0)
    , prevTime(std::chrono::steady_clock::now())
    , startTime(std::chrono::steady_clock::now())
{
}

Game::~Game()
{
    DestroyResources();
}

bool Game::Initialize(DisplayWin32* inDisplay)
{
    this->display = inDisplay;

    // Инициализация D3D11 устройства
    D3D_FEATURE_LEVEL featureLevel[] = { D3D_FEATURE_LEVEL_11_1 };

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Width = display->ClientWidth;
    swapDesc.BufferDesc.Height = display->ClientHeight;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = display->hWnd;
    swapDesc.Windowed = true;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapDesc,
        &swapChain,
        &device,
        nullptr,
        &context
    );

    if (FAILED(res)) {
        std::cout << "D3D11CreateDeviceAndSwapChain failed! HRESULT: " << std::hex << res << '\n';
        return false;
    }

    // Создание InputDevice
    inputDevice = std::make_unique<InputDevice>(display->hWnd);
    // Auto‑register all components that have been linked via SHADDER_COMPONENT
    ecsWorld.AutoRegisterFromRegistry();
    // Создание back buffer и render target view
    CreateBackBuffer();

    return true;
}

void Game::Run()
{
    MSG msg = {};
    bool isExitRequested = false;

    while (!isExitRequested)
    {
        // Обработка Windows сообщений
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                isExitRequested = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!isExitRequested)
        {
            // Обновление времени
            auto curTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(curTime - prevTime).count();
            prevTime = curTime;

            totalTime += deltaTime;
            frameCount++;

            // Обновление FPS в заголовке окна каждую секунду
            if (totalTime > 1.0f)
            {
                float fps = frameCount / totalTime;
                totalTime -= 1.0f;

                WCHAR text[256];
                swprintf_s(text, L"FPS: %f", fps); // NOLINT(cppcoreguidelines-pro-type-vararg)
                SetWindowTextW(display->hWnd, text);

                frameCount = 0;
            }

            // Подготовка кадра
            if (PrepareFrame())
            {
                Update(deltaTime);
                Draw();
                EndFrame();
            }
        }
    }
}

void Game::Exit()
{
    PostQuitMessage(0);
}

bool Game::MessageHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    {
        if (static_cast<unsigned int>(wParam) == 27) // ESC
        {
            Exit();
        }
        return true;
    }
    case WM_SIZE:
    {
        if (wParam != SIZE_MINIMIZED && display)
        {
            display->ClientWidth = LOWORD(lParam);
            display->ClientHeight = HIWORD(lParam);
            ScreenResized(display->ClientWidth, display->ClientHeight);
        }
        return true;
    }
    default:
        return false;
    }
}

void Game::Update(float deltaTime)
{
    UpdateInternal(deltaTime);
    ecsWorld.UpdateSystems(shadder::SystemPhase::UPDATE, deltaTime);
}

void Game::Draw()
{
    // Очистка состояния
    context->ClearState();

    // Восстановление render targets и viewport
    RestoreTargets();

    // Очистка render target
    float color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    context->ClearRenderTargetView(renderTargetView.Get(), color);

    // --- ECS Render Phases ---
    ecsWorld.UpdateSystems(shadder::SystemPhase::PRE_RENDER, 0.0f);
    ecsWorld.UpdateSystems(shadder::SystemPhase::RENDER, 0.0f);
    ecsWorld.UpdateSystems(shadder::SystemPhase::POST_RENDER, 0.0f);
}

void Game::EndFrame()
{
    // Presentation swap chain
    if (swapChain)
    {
        (void)swapChain->Present(1, 0);
    }
}

bool Game::PrepareFrame()
{
    return true;
}

void Game::PrepareResources()
{
    // Переопределяется в наследниках
}

void Game::RestoreTargets()
{
    context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(display->ClientWidth);
    viewport.Height = static_cast<float>(display->ClientHeight);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
}

void Game::UpdateInternal(float deltaTime)
{
    // Внутреннее обновление
}

void Game::CreateBackBuffer()
{
    if (!swapChain || !device || !context)
        return;

    // Получение back buffer
    HRESULT res = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(res))
        return;

    // Создание render target view
    res = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
    if (FAILED(res))
        return;

    // Установка render target и viewport
    RestoreTargets();
}

void Game::DestroyResources()
{
    // Очистка D3D ресурсов
    context.Reset();
    renderTargetView.Reset();
    backBuffer.Reset();
    swapChain.Reset();
    device.Reset();
}

void Game::ScreenResized(int width, int height)
{
    // Пересоздание back buffer при изменении размера окна
    if (context)
    {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        renderTargetView.Reset();
        backBuffer.Reset();

        DXGI_SWAP_CHAIN_DESC swapDesc;
        swapChain->GetDesc(&swapDesc);

        HRESULT res = swapChain->ResizeBuffers(
            swapDesc.BufferCount,
            width,
            height,
            swapDesc.BufferDesc.Format,
            swapDesc.Flags
        );

        if (SUCCEEDED(res))
        {
            CreateBackBuffer();
        }
    }
}
