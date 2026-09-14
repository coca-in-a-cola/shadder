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

Game::Game() : totalTime(0.0f), frameCount(0), prevTime(std::chrono::steady_clock::now()), startTime(std::chrono::steady_clock::now()) {
}

Game::~Game() {
        DestroyResources();
}

Game &Game::SetDisplay(std::unique_ptr<Display> inDisplay) {
        ownedDisplay = std::move(inDisplay);
        display = ownedDisplay.get();
        return *this;
}

Game &Game::SetScreenSize(ScreenSize size) {
        if (size.width > 0 && size.height > 0) {
                screenSize = size;
        }
        return *this;
}

bool Game::Initialize(std::unique_ptr<Display> inDisplay) {
        SetDisplay(std::move(inDisplay));
        return Initialize();
}

bool Game::Initialize() {
        if (!display || !display->Create(screenSize)) {
                std::cout << "Display creation failed!\n";
                return false;
        }
        screenSize = display->GetScreenSize();
        pendingScreenSize = screenSize;
        resizePending = false;
        HWND hwnd = static_cast<HWND>(display->GetNativeHandle());
        if (!hwnd) return false;

        // Инициализация D3D11 устройства
        D3D_FEATURE_LEVEL featureLevel[] = { D3D_FEATURE_LEVEL_11_1 };

        DXGI_SWAP_CHAIN_DESC swapDesc = {};
        swapDesc.BufferCount = 2;
        swapDesc.BufferDesc.Width = screenSize.width;
        swapDesc.BufferDesc.Height = screenSize.height;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.OutputWindow = hwnd;
        swapDesc.Windowed = true;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.SampleDesc.Quality = 0;

         UINT deviceFlags = 0;
#if defined(_DEBUG)
         deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
         HRESULT res = D3D11CreateDeviceAndSwapChain(
                        nullptr,
                        D3D_DRIVER_TYPE_HARDWARE,
                        nullptr,
                        deviceFlags,
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

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;
        if (SUCCEEDED(device.As(&dxgiDevice)) &&
            SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) &&
            SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) {
                factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        }

        // Создание InputDevice
        inputDevice = std::make_unique<InputDevice>(hwnd);

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
                                SetWindowTextW(static_cast<HWND>(display->GetNativeHandle()), text);

                                frameCount = 0;
                        }

                        // Подготовка кадра
                        ApplyPendingResize();
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

bool Game::MessageHandler(void *nativeHandle, std::uintptr_t message,
                          std::uintptr_t wParam, std::intptr_t lParam) {
        HWND hWnd = static_cast<HWND>(nativeHandle);
        const UINT msg = static_cast<UINT>(message);
        const WPARAM nativeWParam = static_cast<WPARAM>(wParam);
        const LPARAM nativeLParam = static_cast<LPARAM>(lParam);
        const bool displayHandled = display && display->HandleNativeMessage(message, wParam, lParam);
        SynchronizeDisplaySize();
        if (displayHandled) return true;

        // Pass messages to ImGui first
        if (imguiInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, msg, nativeWParam, nativeLParam)) {
                return true;
        }

        switch (msg) {
                case WM_KEYDOWN: {
                        if (static_cast<unsigned int>(nativeWParam) == 27) // ESC
                        {
                                Exit();
                        }
                        return true;
                }
                case WM_INPUT: {
                        if (inputDevice) {
                                UINT dwSize = 0;
                                GetRawInputData((HRAWINPUT)nativeLParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
                                LPBYTE lpb = new BYTE[dwSize];
                                if (lpb == nullptr) return 0;

                                if (GetRawInputData((HRAWINPUT)nativeLParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
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

        // Depth belongs to this frame; ClearState does not clear buffer contents.
        if (depthStencilView) {
                context->ClearDepthStencilView(depthStencilView.Get(),
                                               D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                               1.0f, 0);
        }

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
        if (depthStencilView) {
                context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(),
                                            depthStencilView.Get());
        } else {
                context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
        }

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(screenSize.width);
        viewport.Height = static_cast<float>(screenSize.height);
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

        // Depth-stencil буфер под размер back buffer (24-бит depth + 8 stencil).
        D3D11_TEXTURE2D_DESC dsDesc = {};
        dsDesc.Width = static_cast<UINT>(screenSize.width);
        dsDesc.Height = static_cast<UINT>(screenSize.height);
        dsDesc.MipLevels = 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsDesc.SampleDesc.Count = 1;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage = D3D11_USAGE_DEFAULT;
        dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBuffer;
        if (SUCCEEDED(device->CreateTexture2D(&dsDesc, nullptr, depthBuffer.GetAddressOf()))) {
                device->CreateDepthStencilView(depthBuffer.Get(), nullptr,
                                               depthStencilView.GetAddressOf());
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
        if (!ImGui_ImplWin32_Init(display->GetNativeHandle())) {
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
        depthStencilView.Reset();
        backBuffer.Reset();
        swapChain.Reset();
        device.Reset();
}

void Game::ScreenResized(int width, int height) {
        // Пересоздание back buffer при изменении размера окна
        if (context && swapChain && width > 0 && height > 0) {
                context->OMSetRenderTargets(0, nullptr, nullptr);
                renderTargetView.Reset();
                depthStencilView.Reset();
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

void Game::SynchronizeDisplaySize() {
        if (!display) return;
        const ScreenSize displaySize = display->GetScreenSize();
        if (displaySize.width > 0 && displaySize.height > 0 &&
            (displaySize.width != screenSize.width || displaySize.height != screenSize.height)) {
                pendingScreenSize = displaySize;
                resizePending = true;
        }
}

void Game::ApplyPendingResize() {
        SynchronizeDisplaySize();
        if (!resizePending) return;
        screenSize = pendingScreenSize;
        resizePending = false;
        ScreenResized(screenSize.width, screenSize.height);
}
