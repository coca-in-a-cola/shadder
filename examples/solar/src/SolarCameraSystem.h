#pragma once

// SolarCameraSystem — две камеры в одном (SH-S1 п.4-5):
//   FPS    — WASD + мышь (как FPSCharacter в Godot: взгляд мышью, ходьба
//            в горизонтальной плоскости), Q/E — вниз/вверх, Shift — ускорение;
//   ORBIT  — как камера редактора Godot: RMB — взгляд, колесо — зум,
//            MMB — панорама.
// Переключение режимов: TAB (задокументировано в выводе main).
//
// Пресеты матрицы проекции (независимы от режима камеры):
//   1 — PERSPECTIVE FOV 45°,  2 — PERSPECTIVE FOV 90°,  3 — ORTHO (мировые ед.),
//   P — циклическое переключение пресетов. Активный пресет печатается в stdout.
//
// Система работает в фазе UPDATE (получает настоящий dt от Game::Update),
// пишет eye/target/up в CameraComponent; матрицы строит CameraSystem (PRE_RENDER).

#include "core/ecs/System.h"
#include "SolarComponents.h"
#include <DirectXMath.h>

class Game;

class SolarCameraSystem : public ISystem {
public:
    explicit SolarCameraSystem(Game* game) : game_(game) {}

    void OnUpdate(World& world, float deltaTime) override;

private:
    enum class Mode { FPS, ORBIT };

    void ApplyPreset(World& world, SolarSettings& settings, SolarSettings::ProjectionPreset preset);
    void UpdateFPS(World& world, CameraComponent& cam, float dt);
    void UpdateOrbit(World& world, CameraComponent& cam, float dt);

    Game* game_;
    Mode mode_ = Mode::FPS;

    // Состояние FPS-камеры.
    DirectX::XMFLOAT3 fpsPos_ = { 0.0f, 12.0f, -65.0f };
    float fpsYaw_ = 0.0f;     // 0 = взгляд на +Z (к солнцу)
    float fpsPitch_ = -0.15f; // чуть сверху

    // Состояние орбитальной камеры.
    DirectX::XMFLOAT3 orbitTarget_ = { 0.0f, 0.0f, 0.0f };
    float orbitDistance_ = 95.0f;
    float orbitYaw_ = 0.7f;
    float orbitPitch_ = 0.5f;

    // Накопленные за кадр дельты мыши (сигнал Raw Input, иначе теряются события).
    float mouseDX_ = 0.0f;
    float mouseDY_ = 0.0f;
    int   wheelAccum_ = 0;

    // Крайние состояния клавиш (чтобы переключение было по нажатию, не по удержанию).
    bool tabDown_ = false;
    bool pDown_ = false;
    bool k1Down_ = false;
    bool k2Down_ = false;
    bool k3Down_ = false;
};
