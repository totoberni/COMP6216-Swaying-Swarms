#pragma once

#include <cstdint>
#include <vector>
#include "components.h"

struct BoidRenderData {
    float x, y;
    float angle;
    uint32_t color;
    float radius;
    bool is_doctor;
};

struct RenderState {
    std::vector<BoidRenderData> boids;
    SimStats stats;
};
