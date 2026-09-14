#include "PaddleSystem.h"
#include "PongComponents.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "core/ecs/Query.h"
#include <algorithm>

static constexpr float kPaddleBaseH = 120.0f;
static constexpr float kPaddleHalfW = 6.0f;
static constexpr float kPaddleMargin = 10.0f;
static constexpr float kPaddleSpeed = 1000.0f; // pixels/sec

PaddleSystem::PaddleSystem(Game* g) : game_(g) {}

void PaddleSystem::OnUpdate(World& world, float deltaTime) {
    auto* input = game_->GetInputDevice();
    if (!input) return;
    const ScreenSize screenSize = game_->GetScreenSize();
    const float screenW = static_cast<float>(screenSize.width);
    const float screenH = static_cast<float>(screenSize.height);

    float paddleScale = 1.0f;
    Query<PongStateComponent> stateQ(world);
    stateQ.ForEach([&](Entity, PongStateComponent& state) {
        paddleScale = state.paddleScale;
    });
    const float half = kPaddleBaseH * 0.5f * paddleScale;

    // --- Player paddle (W/S) ---
    Query<Transform3D, PlayerPaddleTag> playerQ(world);
    playerQ.ForEach([&](Entity, Transform3D& tr, PlayerPaddleTag&) {
        tr.position.x = kPaddleHalfW + kPaddleMargin;
        tr.scale.y = paddleScale;
        tr.position.y = std::max(half, std::min(screenH - half, tr.position.y));
        if (input->IsKeyDown(Keys::W)) {
            tr.position.y = std::max(half, tr.position.y - kPaddleSpeed * deltaTime);
        }
        if (input->IsKeyDown(Keys::S)) {
            tr.position.y = std::min(screenH - half, tr.position.y + kPaddleSpeed * deltaTime);
        }
    });

    // --- AI paddle (follow ball Y) ---
    // Find ball Y
    float ballY = screenH / 2.0f;
    Query<Transform3D, BallTag> ballQ(world);
    ballQ.ForEach([&](Entity, Transform3D& tr, BallTag&) {
        ballY = tr.position.y;
    });

    Query<Transform3D, AiPaddleTag> aiQ(world);
    aiQ.ForEach([&](Entity, Transform3D& tr, AiPaddleTag&) {
        tr.position.x = screenW - kPaddleHalfW - kPaddleMargin;
        tr.scale.y = paddleScale;
        tr.position.y = std::max(half, std::min(screenH - half, tr.position.y));
        float diff = ballY - tr.position.y;
        if (std::abs(diff) > 10.0f) {
            float step = kPaddleSpeed * deltaTime;
            if (diff > 0)
                tr.position.y = std::min(screenH - half, tr.position.y + step);
            else
                tr.position.y = std::max(half, tr.position.y - step);
        }
    });
}
