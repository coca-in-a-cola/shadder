#include "ScoreTextSystem.h"

#include "core/ecs/Query.h"
#include "framework/modules/render/TextComponent.h"

#include "PongComponents.h"
#include "ScoreTextComponent.h"

#include <string>

using shadder::TextComponent;

void ScoreTextSystem::OnUpdate(World& world, float) {
    Query<PongStateComponent> stateQ(world);
    stateQ.ForEach([&](Entity, PongStateComponent& s) {
        // Rebuild text only when something visible changed.
        if (s.score1 == lastScore1_ && s.score2 == lastScore2_ &&
            static_cast<int>(s.state) == lastState_) {
            return;
        }
        lastScore1_ = s.score1;
        lastScore2_ = s.score2;
        lastState_ = static_cast<int>(s.state);

        Query<TextComponent, ScoreTextComponent> textQ(world);
        textQ.ForEach([&](Entity, TextComponent& text, ScoreTextComponent& tag) {
            switch (tag.slot) {
                case 0: // left score
                    text.text = std::to_string(s.score1);
                    break;
                case 1: // right score
                    text.text = std::to_string(s.score2);
                    break;
                case 2: // center message
                    if (s.state == PongStateComponent::COOLDOWN) {
                        text.text = "GO!";
                        text.color = { 1.0f, 1.0f, 0.0f, 1.0f };
                    } else if (s.state == PongStateComponent::GAMEOVER) {
                        text.text = (s.score1 > s.score2) ? "WINNER LEFT" : "WINNER RIGHT";
                        text.color = { 0.0f, 1.0f, 0.0f, 1.0f };
                    } else {
                        text.text = "";
                    }
                    break;
                default:
                    break;
            }
        });
    });
}
