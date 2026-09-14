#include "KatamariSettingsUI.h"

#include "KatamariComponents.h"
#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "core/ecs/Query.h"

#include <imgui.h>

void KatamariSettingsUI::OnUpdate(World& world, float) {
    if (!game_) return;

    bool f1Now = false;
    if (InputDevice* input = game_->GetInputDevice()) {
        f1Now = input->IsKeyDown(Keys::F1);
    }
    if (f1Now && !f1Prev_) open_ = !open_;
    f1Prev_ = f1Now;

    if (!open_) return;

    float radius = 0.0f;
    int collected = 0;
    int total = 0;
    Query<KatamariBallComponent> ballQ(world);
    ballQ.ForEach([&](Entity, KatamariBallComponent& ball) { radius = ball.radius; });
    Query<KatamariStatsComponent> statsQ(world);
    statsQ.ForEach([&](Entity, KatamariStatsComponent& stats) {
        collected = stats.collected;
        total = stats.total;
    });

    ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Katamari Help")) {
        ImGui::Text("F1 - toggle this menu");
        ImGui::SeparatorText("Controls");
        ImGui::BulletText("W/S: move forward/backward");
        ImGui::BulletText("A/D: tank steering");
        ImGui::BulletText("Mouse wheel: camera zoom");
        ImGui::BulletText("ESC: exit");
        ImGui::SeparatorText("Status");
        ImGui::Text("Radius: %.2f", radius);
        ImGui::Text("Collected: %d / %d", collected, total);
    }
    ImGui::End();
}
