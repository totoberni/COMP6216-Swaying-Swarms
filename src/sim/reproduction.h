#pragma once

#include <random>

// Pure C++ reproduction logic — no FLECS includes

// Calculate number of offspring from Normal distribution
// Returns max(0, round(Normal(mean, stddev)))
int offspring_count(float mean, float stddev, std::mt19937& rng);

// Attempt reproduction with given probability
// Returns true if reproduction succeeds
bool try_reproduce(float p_offspring, std::mt19937& rng);
