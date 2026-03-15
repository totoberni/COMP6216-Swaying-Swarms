#pragma once

#include "gpu/sim_config_gpu.h"
#include <string>
#include <cstdint>

// GPU headless simulation — takes pre-extracted SoA arrays (from FLECS)
// Caller is responsible for FLECS entity extraction (avoids raylib/CUDA header conflict)
void gpu_run_headless(
    // SoA entity data (host arrays, count elements each)
    int count, int normal_count, int doctor_count,
    float* h_pos_x, float* h_pos_y,
    float* h_vel_x, float* h_vel_y,
    uint8_t* h_swarm_type, uint8_t* h_infected, float* h_immunity,
    // Simulation config
    const SimConfigGpu& cfg,
    float duration, float dt, float csv_interval,
    // Output
    const char* output_dir, const std::string& config_path);
