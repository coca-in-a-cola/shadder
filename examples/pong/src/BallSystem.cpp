#include "BallSystem.h"
#include "PongComponents.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/physics/VelocityComponent.h"
#include "core/ecs/Query.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

BallSystem::BallSystem(float screenW, float screenH,
                       float paddleW, float paddleH,
                       float ballSize)
    : screenW_(screenW), screenH_(screenH),
      paddleW_(paddleW), paddleH_(paddleH),
      ballSize_(ballSize) {}

float BallSystem::DifficultyCurve(int ballHits) {
    return 5.0f / (1.0f + std::exp(-static_cast<float>(ballHits) / 8.0f + 4.0f)) + 1.0f;
}

static bool AABBOverlap(float ax, float ay, float aw, float ah,
                        float bx, float by, float bw, float bh) {
    return std::abs(ax - bx) < (aw + bw) * 0.5f &&
           std::abs(ay - by) < (ah + bh) * 0.5f;
}

void BallSystem::OnUpdate(World& world, float deltaTime) {
    // Find state entity
    PongStateComponent* state = nullptr;
    Query<PongStateComponent> stateQ(world);
    stateQ.ForEach([&](Entity, PongStateComponent& s) {
        state = &s;
    });
    if (!state) return;

    // Handle cooldown
    if (state->state == PongStateComponent::COOLDOWN) {
        if (std::chrono::steady_clock::now() >= state->continueTime) {
            state->state = PongStateComponent::NORMAL;
        }
        return;
    }

    if (state->state == PongStateComponent::GAMEOVER) return;

    // Update speed from difficulty curve
    float speed = 300.0f * DifficultyCurve(state->ballHits);

    // Find ball
    Transform3D* ballTr = nullptr;
    VelocityComponent* ballVel = nullptr;
    Query<Transform3D, VelocityComponent, BallTag> ballQ(world);
    ballQ.ForEach([&](Entity, Transform3D& tr, VelocityComponent& vel, BallTag&) {
        ballTr = &tr;
        ballVel = &vel;
    });
    if (!ballTr || !ballVel) return;

    // Normalize direction and apply speed
    float dx = ballVel->velocity.x;
    float dy = ballVel->velocity.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    ballVel->velocity.x = dx / len * speed;
    ballVel->velocity.y = dy / len * speed;

    float nextX = ballTr->position.x + ballVel->velocity.x * deltaTime;
    float nextY = ballTr->position.y + ballVel->velocity.y * deltaTime;

    // Wall bounce (top/bottom)
    if (nextY - ballSize_ * 0.5f < 0.0f) {
        nextY = ballSize_ * 0.5f;
        ballVel->velocity.y = std::abs(ballVel->velocity.y);
    } else if (nextY + ballSize_ * 0.5f > screenH_) {
        nextY = screenH_ - ballSize_ * 0.5f;
        ballVel->velocity.y = -std::abs(ballVel->velocity.y);
    }

    // Score when ball goes past left/right edges
    if (nextX + ballSize_ * 0.5f < 0.0f) {
        state->score2 += 1;
        state->ballHits = 0;
        state->state = PongStateComponent::COOLDOWN;
        state->continueTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        ballTr->position.x = screenW_ / 2.0f;
        ballTr->position.y = screenH_ / 2.0f;
        ballVel->velocity.x = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
        ballVel->velocity.y = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;
        float l = std::sqrt(ballVel->velocity.x * ballVel->velocity.x +
                           ballVel->velocity.y * ballVel->velocity.y);
        ballVel->velocity.x /= l;
        ballVel->velocity.y /= l;
        if (state->score2 > 10) state->state = PongStateComponent::GAMEOVER;
        return;
    }
    if (nextX - ballSize_ * 0.5f > screenW_) {
        state->score1 += 1;
        state->ballHits = 0;
        state->state = PongStateComponent::COOLDOWN;
        state->continueTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        ballTr->position.x = screenW_ / 2.0f;
        ballTr->position.y = screenH_ / 2.0f;
        ballVel->velocity.x = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
        ballVel->velocity.y = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f;
        float l = std::sqrt(ballVel->velocity.x * ballVel->velocity.x +
                           ballVel->velocity.y * ballVel->velocity.y);
        ballVel->velocity.x /= l;
        ballVel->velocity.y /= l;
        if (state->score1 > 10) state->state = PongStateComponent::GAMEOVER;
        return;
    }

    // Paddle collision (player paddle - left)
    Query<Transform3D, PlayerPaddleTag> p1Q(world);
    p1Q.ForEach([&](Entity, Transform3D& tr, PlayerPaddleTag&) {
        if (ballVel->velocity.x < 0 && AABBOverlap(
                nextX, nextY, ballSize_, ballSize_,
                tr.position.x, tr.position.y, paddleW_, paddleH_)) {
            float hitPos = (nextY - tr.position.y) / (paddleH_ * 0.5f);
            hitPos = std::max(-1.0f, std::min(1.0f, hitPos));
            float nx = 1.0f, ny = hitPos * 0.6f;
            float nl = std::sqrt(nx * nx + ny * ny);
            ballVel->velocity.x = nx / nl * speed;
            ballVel->velocity.y = ny / nl * speed;
            state->ballHits += 1;
        }
    });

    // Paddle collision (AI paddle - right)
    Query<Transform3D, AiPaddleTag> p2Q(world);
    p2Q.ForEach([&](Entity, Transform3D& tr, AiPaddleTag&) {
        if (ballVel->velocity.x > 0 && AABBOverlap(
                nextX, nextY, ballSize_, ballSize_,
                tr.position.x, tr.position.y, paddleW_, paddleH_)) {
            float hitPos = (nextY - tr.position.y) / (paddleH_ * 0.5f);
            hitPos = std::max(-1.0f, std::min(1.0f, hitPos));
            float nx = -1.0f, ny = hitPos * 0.6f;
            float nl = std::sqrt(nx * nx + ny * ny);
            ballVel->velocity.x = nx / nl * speed;
            ballVel->velocity.y = ny / nl * speed;
            state->ballHits += 1;
        }
    });

    ballTr->position.x = nextX;
    ballTr->position.y = nextY;
}
