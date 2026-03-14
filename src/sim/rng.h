#pragma once

#include <random>

// Thread-local RNG — each thread gets its own independent engine
inline std::mt19937& sim_rng() {
    thread_local std::mt19937 engine(42);
    return engine;
}

// Seed this thread's RNG (call at start of each sweep run)
inline void seed_sim_rng(uint32_t seed) {
    sim_rng().seed(seed);
}
