#pragma once

#include <random>

// Pure C++ cure logic — no FLECS includes

// Attempt cure with given probability
// Returns true if cure succeeds
bool try_cure(float p_cure, std::mt19937& rng);
