#pragma once

#include "core/ecs/System.h"

class Game;

class SolarSettingsUI final : public ISystem {
  public:
    explicit SolarSettingsUI(Game* game) : game_(game) {}
    void OnUpdate(World& world, float deltaTime) override;

  private:
    Game* game_;
    bool open_ = true;
    bool f1Prev_ = false;
};
