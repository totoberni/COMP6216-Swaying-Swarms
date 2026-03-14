#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct SweepConfig {
    std::string base_config_path;
    std::string output_dir = "sim-out";
    int n_samples = 600;
    int threads = 12;
    uint32_t seed = 42;
    float duration = 522.0f;
};

// Main entry point — called from main.cpp
void run_sweep(const SweepConfig& config);
