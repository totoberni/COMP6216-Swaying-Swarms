#pragma once

#include <cstdint>
#include <vector>
#include "components.h"

struct BoidRenderData {
    float x, y;
    float angle;
    uint32_t color;
    float radius;
    int swarm_type;  // 0=normal, 1=doctor, 2=antivax
};

struct RenderState {
    std::vector<BoidRenderData> boids;
    SimStats stats;
    SimConfig* config = nullptr;
    SimulationState* sim_state = nullptr;
};
