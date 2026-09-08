#pragma once

#include "core/ecs/System.h"

// Keeps the SH-0 score/message text entities in sync with PongStateComponent.
// Runs in the UPDATE phase: without it the on-screen score would only be
// written once at startup (and ImGui Reset wouldn't be visible).
class ScoreTextSystem : public ISystem {
  public:
  void OnUpdate(World& world, float deltaTime) override;

  private:
  int lastScore1_ = -1;
  int lastScore2_ = -1;
  int lastState_ = -1;
};
