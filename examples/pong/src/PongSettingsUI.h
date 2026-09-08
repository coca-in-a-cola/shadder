#pragma once

#include "core/ecs/System.h"

class Game;

// Demo of the ImGui integration (SH-U). F1 toggles the "Pong Settings" window:
// ball speed multiplier, paddle size, target score, Reset button and the
// current score (rendered on screen by the SH-0 text system).
class PongSettingsUI : public ISystem {
  public:
  explicit PongSettingsUI(Game* g) : game_(g) {}
  void OnUpdate(World& world, float deltaTime) override;

  private:
  Game* game_;
  bool open_ = false;
  bool f1Prev_ = false;
};
