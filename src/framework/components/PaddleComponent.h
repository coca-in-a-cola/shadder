#pragma once
#include "ecs/Component.h"


struct PaddleComponent : public ComponentBase {
    SHADDER_COMPONENT_HEADER(PaddleComponent)
public:
    int playerID = 0; // 0 = left, 1 = right
};


