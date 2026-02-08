#pragma once

#include <random>

// Pure C++ promotion logic — no FLECS includes

// Check if a normal boid should be promoted to doctor
// Requires age >= t_adult AND random chance p_become_doctor
bool try_promote(float age, float t_adult, float p_become_doctor, std::mt19937& rng);
