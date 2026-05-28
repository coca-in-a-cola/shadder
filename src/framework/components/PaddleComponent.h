#pragma once
#include "ecs/Component.h"

namespace shadder {

struct PaddleComponent : public ComponentBase {
    SHADDER_COMPONENT_HEADER(PaddleComponent)
public:
    int playerID = 0; // 0 = left, 1 = right
};

} // namespace shadder
