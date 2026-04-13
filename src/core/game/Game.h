#pragma once

#include "../InputDevice.h"
#include "../DisplayWin32.h"
#include "GameComponent.h"
#include <chrono>
#include <d3d11.h>
#include <dxgi.h>
#include <memory>
#include <vector>
#include <wrl.h>
#include <string>

class Game {
  friend class GameComponent;
  friend class InputDevice;

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

  // Компоненты
  void AddComponent(std::unique_ptr<GameComponent> component);

  // Публичные геттеры
  ID3D11Device* GetDevice() { return device.Get(); }
  ID3D11DeviceContext* GetContext() { return context.Get(); }
  IDXGISwapChain* GetSwapChain() { return swapChain.Get(); }
  ID3D11RenderTargetView* GetRenderTargetView() { return renderTargetView.Get(); }
  InputDevice* GetInputDevice() { return inputDevice.get(); }
  DisplayWin32* GetDisplay() { return display; }
  float GetTotalTime() const { return totalTime; }
  HWND GetHWnd() const { return display ? display->hWnd : nullptr; }

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

  // Имя приложения
  std::string name;

  // Display (owned externally)
  DisplayWin32* display;

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

  // Компоненты
  std::vector<std::unique_ptr<GameComponent>> components;
};
