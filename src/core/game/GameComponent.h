#pragma once

class Game;

class GameComponent {
public:
    GameComponent(Game* inGame);
    virtual ~GameComponent() = default;

    virtual void Initialize() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;
    virtual void Reload() {}
    virtual void DestroyResources() {}

protected:
    Game* game;
};
