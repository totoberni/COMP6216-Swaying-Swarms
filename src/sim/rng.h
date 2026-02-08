#pragma once

#include <random>

// Shared seeded random engine for all simulation logic
// Using inline to ensure single instance across translation units
inline std::mt19937& sim_rng() {
    static std::mt19937 engine(42);  // Fixed seed for reproducibility
    return engine;
}
