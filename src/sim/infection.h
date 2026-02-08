#pragma once

#include <random>

// Pure C++ infection logic — no FLECS includes

// Attempt infection with given probability
// Returns true if infection succeeds
bool try_infect(float p_infect, std::mt19937& rng);
