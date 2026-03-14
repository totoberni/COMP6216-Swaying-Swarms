#pragma once

#include <random>

// Pure C++ infection logic — no FLECS includes

// Attempt infection with per-second probability, dt-scaled to per-frame
// Returns true if infection succeeds
bool try_infect(float p_per_second, float dt, std::mt19937& rng);
