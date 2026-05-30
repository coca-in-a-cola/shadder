#include "PaddleSystem.h"
#include "PongComponents.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "ecs/Query.h"
#include <algorithm>

static constexpr float kPaddleHalfH = 80.0f; // half-height in pixels
static constexpr float kPaddleSpeed = 500.0f; // pixels/sec

PaddleSystem::PaddleSystem(Game* g, float screenH)
    : game_(g), screenH_(screenH) {}

void PaddleSystem::OnUpdate(World& world, float deltaTime) {
    auto* input = game_->GetInputDevice();
    if (!input) return;

    // --- Player paddle (W/S) ---
    Query<Transform3D, PlayerPaddleTag> playerQ(world);
    playerQ.ForEach([&](Entity, Transform3D& tr, PlayerPaddleTag&) {
        float half = kPaddleHalfH;
        if (input->IsKeyDown(Keys::W)) {
            tr.position.y = std::max(half, tr.position.y - kPaddleSpeed * deltaTime);
        }
        if (input->IsKeyDown(Keys::S)) {
            tr.position.y = std::min(screenH_ - half, tr.position.y + kPaddleSpeed * deltaTime);
        }
    });

    // --- AI paddle (follow ball Y) ---
    // Find ball Y
    float ballY = screenH_ / 2.0f;
    Query<Transform3D, BallTag> ballQ(world);
    ballQ.ForEach([&](Entity, Transform3D& tr, BallTag&) {
        ballY = tr.position.y;
    });

    Query<Transform3D, AiPaddleTag> aiQ(world);
    aiQ.ForEach([&](Entity, Transform3D& tr, AiPaddleTag&) {
        float half = kPaddleHalfH;
        float diff = ballY - tr.position.y;
        if (std::abs(diff) > 10.0f) {
            float step = kPaddleSpeed * deltaTime;
            if (diff > 0)
                tr.position.y = std::min(screenH_ - half, tr.position.y + step);
            else
                tr.position.y = std::max(half, tr.position.y - step);
        }
    });
}
