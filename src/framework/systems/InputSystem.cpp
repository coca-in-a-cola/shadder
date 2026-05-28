#include "framework/systems/InputSystem.h"
#include "framework/game/Game.h"
#include "dev/input/InputDevice.h"
#include "shared/keycodes/Keys.h"
#include "ecs/Query.h"
#include "framework/components/PaddleComponent.h"
#include "framework/components/VelocityComponent.h"


void InputSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* input = game_->GetInputDevice();
    if (!input) return;

    Query<PaddleComponent, VelocityComponent> q(world);
    q.ForEach([input](Entity, PaddleComponent& paddle, VelocityComponent& vel) {
        const float speed = 300.0f;
        if (paddle.playerID == 0) { // Left paddle: W/S
            if (input->IsKeyDown(Keys::W)) vel.velocity.y = speed;
            else if (input->IsKeyDown(Keys::S)) vel.velocity.y = -speed;
            else vel.velocity.y = 0;
        } else { // Right paddle: Up/Down
            if (input->IsKeyDown(Keys::Up)) vel.velocity.y = speed;
            else if (input->IsKeyDown(Keys::Down)) vel.velocity.y = -speed;
            else vel.velocity.y = 0;
        }
    });
}


