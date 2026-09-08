#include "framework/game/Game.h"
#include <iostream>
#include <windows.h>

#include "framework/modules/physics/register_types.h"
#include "framework/modules/render/register_types.h"
#include "framework/modules/transform/register_types.h"
#include "framework/modules/camera/register_types.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

// Forward-declared in backend header inside '#if 0' (avoids <windows.h> dependency there)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dwmapi.lib")

Game::Game() : display(nullptr), totalTime(0.0f), frameCount(0), prevTime(std::chrono::steady_clock::now()), startTime(std::chrono::steady_clock::now()) {
}

Game::~Game() {
        DestroyResources();
}

bool Game::Initialize(DisplayWin32 *inDisplay) {
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
                        &context);

        if (FAILED(res)) {
                std::cout << "D3D11CreateDeviceAndSwapChain failed! HRESULT: " << std::hex << res << '\n';
                return false;
        }

        // Создание InputDevice
        inputDevice = std::make_unique<InputDevice>(display->hWnd);

        // Initialize ECS modules explicitly (Godot-style)
        initialize_transform_module(ecsWorld);
        initialize_render_module(ecsWorld);
        initialize_physics_module(ecsWorld);
        initialize_camera_module(ecsWorld);

        // Создание back buffer и render target view
        CreateBackBuffer();

        // Initialize ImGui
        if (!InitializeImGui()) {
                std::cout << "Failed to initialize ImGui!\n";
                return false;
        }

        return true;
}

void Game::Run() {
        MSG msg = {};
        bool isExitRequested = false;

        while (!isExitRequested) {
                // Обработка Windows сообщений
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                        if (msg.message == WM_QUIT) {
                                isExitRequested = true;
                                break;
                        }
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                }

                if (!isExitRequested) {
                        // Обновление времени
                        auto curTime = std::chrono::steady_clock::now();
                        float deltaTime = std::chrono::duration<float>(curTime - prevTime).count();
                        prevTime = curTime;

                        totalTime += deltaTime;
                        frameCount++;

                        // Обновление FPS в заголовке окна каждую секунду
                        if (totalTime > 1.0f) {
                                float fps = frameCount / totalTime;
                                totalTime -= 1.0f;

                                WCHAR text[256];
                                swprintf_s(text, L"FPS: %f", fps); // NOLINT(cppcoreguidelines-pro-type-vararg)
                                SetWindowTextW(display->hWnd, text);

                                frameCount = 0;
                        }

                        // Подготовка кадра
                        if (PrepareFrame()) {
                                Update(deltaTime);
                                Draw();
                                EndFrame();
                        }
                }
        }
}

void Game::Exit() {
        PostQuitMessage(0);
}

bool Game::MessageHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // Pass messages to ImGui first
        if (imguiInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
                return true;
        }

        switch (msg) {
                case WM_KEYDOWN: {
                        if (static_cast<unsigned int>(wParam) == 27) // ESC
                        {
                                Exit();
                        }
                        return true;
                }
                case WM_INPUT: {
                        if (inputDevice) {
                                UINT dwSize = 0;
                                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
                                LPBYTE lpb = new BYTE[dwSize];
                                if (lpb == nullptr) return 0;

                                if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
                                        delete[] lpb;
                                        return 0;
                                }

                                RAWINPUT* raw = (RAWINPUT*)lpb;
                                if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                                        InputDevice::KeyboardInputEventArgs args;
                                        args.MakeCode = raw->data.keyboard.MakeCode;
                                        args.Flags = raw->data.keyboard.Flags;
                                        args.VKey = raw->data.keyboard.VKey;
                                        args.Message = raw->data.keyboard.Message;
                                        inputDevice->OnKeyDown(args);
                                } else if (raw->header.dwType == RIM_TYPEMOUSE) {
                                        InputDevice::RawMouseEventArgs args;
                                        args.Mode = raw->data.mouse.usFlags;
                                        args.ButtonFlags = raw->data.mouse.usButtonFlags;
                                        args.ExtraInformation = raw->data.mouse.ulExtraInformation;
                                        args.Buttons = raw->data.mouse.usButtonFlags;
                                        args.WheelDelta = (short)raw->data.mouse.usButtonData;
                                        args.X = raw->data.mouse.lLastX;
                                        args.Y = raw->data.mouse.lLastY;
                                        inputDevice->OnMouseMove(args);
                                }
                                delete[] lpb;
                                return 0;
                        }
                        break;
                }
                case WM_SIZE: {
                        if (wParam != SIZE_MINIMIZED && display) {
                                display->ClientWidth = LOWORD(lParam);
                                display->ClientHeight = HIWORD(lParam);
                                ScreenResized(display->ClientWidth, display->ClientHeight);
                        }
                        return true;
                }
                default:
                        return false;
        }
        return false;
}

void Game::Update(float deltaTime) {
        UpdateInternal(deltaTime);
        ecsWorld.UpdateSystems(SystemPhase::UPDATE, deltaTime);
}

void Game::Draw() {
        // Очистка состояния
        context->ClearState();

        // Восстановление render targets и viewport
        RestoreTargets();

        // Очистка render target
        float color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        context->ClearRenderTargetView(renderTargetView.Get(), color);

        // --- ECS Render Phases ---
        ecsWorld.UpdateSystems(SystemPhase::PRE_RENDER, 0.0f);
        ecsWorld.UpdateSystems(SystemPhase::RENDER, 0.0f);
        ecsWorld.UpdateSystems(SystemPhase::POST_RENDER, 0.0f);

        // ImGui render
        ImGuiRender();
}

void Game::EndFrame() {
        // Presentation swap chain
        if (swapChain) {
                (void)swapChain->Present(1, 0);
        }
}

bool Game::PrepareFrame() {
        // ImGui new frame
        ImGuiNewFrame();
        return true;
}

void Game::PrepareResources() {
        // Переопределяется в наследниках
}

void Game::RestoreTargets() {
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

void Game::UpdateInternal(float deltaTime) {
        // Внутреннее обновление
}

void Game::CreateBackBuffer() {
        if (!swapChain || !device || !context) {
                return;
        }

        // Получение back buffer
        HRESULT res = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
        if (FAILED(res)) {
                return;
        }

        // Создание render target view
        res = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
        if (FAILED(res)) {
                return;
        }

        // Установка render target и viewport
        RestoreTargets();
}

// ImGui initialization
bool Game::InitializeImGui() {
        IMGUI_CHECKVERSION();
        imguiContext = ImGui::CreateContext();
        if (!imguiContext) {
                return false;
        }
        ImGui::SetCurrentContext(imguiContext);

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Setup backends
        if (!ImGui_ImplWin32_Init(display->hWnd)) {
                std::cout << "ImGui_ImplWin32_Init failed!\n";
                return false;
        }

        if (!ImGui_ImplDX11_Init(device.Get(), context.Get())) {
                std::cout << "ImGui_ImplDX11_Init failed!\n";
                ImGui_ImplWin32_Shutdown();
                return false;
        }

        imguiInitialized = true;
        return true;
}

void Game::ShutdownImGui() {
        if (imguiInitialized) {
                ImGui_ImplDX11_Shutdown();
                ImGui_ImplWin32_Shutdown();
                if (imguiContext) {
                        ImGui::DestroyContext(imguiContext);
                        imguiContext = nullptr;
                }
                imguiInitialized = false;
        }
}

void Game::ImGuiNewFrame() {
        if (!imguiInitialized) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
}

void Game::ImGuiRender() {
        if (!imguiInitialized) return;

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Game::DestroyResources() {
        // Shutdown ImGui first
        ShutdownImGui();

        // Очистка D3D ресурсов
        context.Reset();
        renderTargetView.Reset();
        backBuffer.Reset();
        swapChain.Reset();
        device.Reset();
}

void Game::ScreenResized(int width, int height) {
        // Пересоздание back buffer при изменении размера окна
        if (context) {
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
                                swapDesc.Flags);

                if (SUCCEEDED(res)) {
                        CreateBackBuffer();
                }
        }
}
