#include "cure.h"
#include <cmath>

bool try_cure(float p_per_second, float dt, std::mt19937& rng) {
    // Convert per-second probability to per-frame (frame-rate invariant)
    float p_per_frame = 1.0f - std::pow(1.0f - p_per_second, dt);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < p_per_frame;
}
