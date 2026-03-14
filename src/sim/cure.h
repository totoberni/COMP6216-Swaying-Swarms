#pragma once

#include <random>

// Pure C++ cure logic — no FLECS includes

// Attempt cure with per-second probability, dt-scaled to per-frame
// Returns true if cure succeeds
bool try_cure(float p_per_second, float dt, std::mt19937& rng);
