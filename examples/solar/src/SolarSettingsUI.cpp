#include "SolarSettingsUI.h"

#include "SolarComponents.h"
#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "core/ecs/Query.h"

#include <imgui.h>

namespace {

const char* ProjectionName(SolarSettings::ProjectionPreset preset) {
    switch (preset) {
        case SolarSettings::ProjectionPreset::FOV45: return "Perspective 45 deg";
        case SolarSettings::ProjectionPreset::FOV90: return "Perspective 90 deg";
        case SolarSettings::ProjectionPreset::ORTHO: return "Orthographic";
    }
    return "Unknown";
}

} // namespace

void SolarSettingsUI::OnUpdate(World& world, float) {
    if (!game_) return;

    bool f1Now = false;
    if (InputDevice* input = game_->GetInputDevice()) {
        f1Now = input->IsKeyDown(Keys::F1);
    }
    if (f1Now && !f1Prev_) open_ = !open_;
    f1Prev_ = f1Now;

    SolarSettings* settings = nullptr;
    Query<SolarSettings> settingsQ(world);
    settingsQ.ForEach([&](Entity, SolarSettings& value) { settings = &value; });
    if (!settings || !open_) return;

    ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Solar System Settings")) {
        ImGui::Text("F1 - toggle this menu");
        ImGui::SeparatorText("Camera");

        int cameraMode = settings->cameraFPS ? 0 : 1;
        if (ImGui::RadioButton("FPS camera", &cameraMode, 0)) {
            settings->cameraFPS = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Orbit camera", &cameraMode, 1)) {
            settings->cameraFPS = false;
        }

        ImGui::Text("Projection: %s", ProjectionName(settings->projection));
        ImGui::SeparatorText("Projection matrix");

        int projection = static_cast<int>(settings->projection);
        if (ImGui::RadioButton("Perspective 45 deg", &projection, 0)) {
            settings->projection = SolarSettings::ProjectionPreset::FOV45;
        }
        if (ImGui::RadioButton("Perspective 90 deg", &projection, 1)) {
            settings->projection = SolarSettings::ProjectionPreset::FOV90;
        }
        if (ImGui::RadioButton("Orthographic", &projection, 2)) {
            settings->projection = SolarSettings::ProjectionPreset::ORTHO;
        }

        if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (settings->cameraFPS) {
                ImGui::BulletText("Mouse: look around");
                ImGui::BulletText("WASD: move, Q/E: down/up");
                ImGui::BulletText("Shift: move faster");
            } else {
                ImGui::BulletText("RMB + mouse: orbit camera");
                ImGui::BulletText("Mouse wheel or W/S: zoom");
                ImGui::BulletText("MMB + mouse: pan target");
            }
            ImGui::BulletText("TAB: switch camera");
            ImGui::BulletText("1/2/3 or P: switch projection");
            ImGui::BulletText("ESC: exit");
        }
    }
    ImGui::End();
}
