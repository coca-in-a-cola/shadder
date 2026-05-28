#pragma once

#include "dev/input/InputDevice.h"
#include "dev/display/DisplayWin32.h"
#include "ecs/World.h"
#include <chrono>
#include <d3d11.h>
#include <dxgi.h>
#include <memory>
#include <vector>
#include <wrl.h>
#include <string>

class Game {
public:
  Game();
  virtual ~Game();

  // Инициализация и основной цикл
  bool Initialize(DisplayWin32* display);
  void Run();
  void Exit();

  // Обработка сообщений Windows
  bool MessageHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // Внутренние методы для управления кадрами
  void Update(float deltaTime);
  void Draw();

  // Публичные геттеры
  [[nodiscard]] ID3D11Device* GetDevice() const { return device.Get(); }
  [[nodiscard]] ID3D11DeviceContext* GetContext() const { return context.Get(); }
  [[nodiscard]] IDXGISwapChain* GetSwapChain() const { return swapChain.Get(); }
  [[nodiscard]] ID3D11RenderTargetView* GetRenderTargetView() const { return renderTargetView.Get(); }
  [[nodiscard]] InputDevice* GetInputDevice() const { return inputDevice.get(); }
  [[nodiscard]] DisplayWin32* GetDisplay() const { return display; }
    [[nodiscard]] shadder::World& GetWorld() { return ecsWorld; }
    [[nodiscard]] const shadder::World& GetWorld() const { return ecsWorld; }
  [[nodiscard]] HWND GetHWnd() const { return display ? display->hWnd : nullptr; }

protected:
  // Виртуальные методы для переопределения в наследниках
  virtual bool PrepareFrame();
  virtual void PrepareResources();
  virtual void RestoreTargets();

  // Внутренние методы
  void UpdateInternal(float deltaTime);
  void EndFrame();

  // Создание/уничтожение ресурсов
  virtual void CreateBackBuffer();
  virtual void DestroyResources();

  // Экран изменил размер
  virtual void ScreenResized(int width, int height);

  // Display (owned externally, not destroyed by Game)
    DisplayWin32* display;
    // ECS world – lives inside Game
    shadder::World ecsWorld;

  // D3D11 устройства
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;

  // Ввод
  std::unique_ptr<InputDevice> inputDevice;

  // Временные переменные
  std::chrono::time_point<std::chrono::steady_clock> prevTime;
  std::chrono::time_point<std::chrono::steady_clock> startTime;
  float totalTime;
  unsigned int frameCount;
};
