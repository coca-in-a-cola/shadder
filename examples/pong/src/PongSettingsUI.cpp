#include "PongSettingsUI.h"

#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "core/ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"

#include "PongComponents.h"

#include <imgui.h>
#include <chrono>

namespace {

constexpr float kScreenW = 800.0f; // keep in sync with main.cpp
constexpr float kScreenH = 800.0f;
constexpr float kBasePaddleH = 120.0f; // QuadPrefab height in main.cpp

} // namespace

void PongSettingsUI::OnUpdate(World& world, float) {
    // --- F1 toggles the window (edge-triggered so holding F1 doesn't flicker) ---
    bool f1Now = false;
    if (InputDevice* input = game_->GetInputDevice()) {
        f1Now = input->IsKeyDown(Keys::F1);
    }
    if (f1Now && !f1Prev_) {
        open_ = !open_;
    }
    f1Prev_ = f1Now;

    // --- Read current settings from the game state ---
    PongStateComponent* state = nullptr;
    Query<PongStateComponent> stateQ(world);
    stateQ.ForEach([&](Entity, PongStateComponent& s) {
        state = &s;
    });
    if (!state) return;

    float speedMultiplier = state->speedMultiplier;
    float paddleHeight = kBasePaddleH * state->paddleScale;

    // --- Draw the window ---
    if (open_) {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Pong Settings", &open_)) {
            ImGui::Text("F1 - toggle this window");
            ImGui::SeparatorText("Rules");

            ImGui::SliderFloat("Ball speed", &speedMultiplier, 0.5f, 3.0f, "%.2fx");
            ImGui::SliderFloat("Paddle size", &paddleHeight, 60.0f, 240.0f, "%.0f px");
            ImGui::SliderInt("Target score", &state->targetScore, 3, 30);

            ImGui::SeparatorText("Match");
            ImGui::Text("Score: %d - %d", state->score1, state->score2);
            ImGui::Text("Rally hits: %d", state->ballHits);
            const char* stateName =
                (state->state == PongStateComponent::COOLDOWN) ? "Cooldown" :
                (state->state == PongStateComponent::GAMEOVER) ? "Game over" : "Playing";
            ImGui::Text("State: %s", stateName);

            ImGui::Separator();
            if (ImGui::Button("Reset", ImVec2(-FLT_MIN, 0.0f))) {
                state->score1 = 0;
                state->score2 = 0;
                state->ballHits = 0;
                state->state = PongStateComponent::COOLDOWN;
                state->continueTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
        }
        ImGui::End();
    }

    // --- Write settings back (immediate mode: apply in the same frame) ---
    state->speedMultiplier = speedMultiplier;
    state->paddleScale = paddleHeight / kBasePaddleH;
}
