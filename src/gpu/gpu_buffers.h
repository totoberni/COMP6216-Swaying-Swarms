#pragma once

#include <cstdint>
#include <cstddef>

struct GpuBuffers {
    // Primary SoA arrays (sorted by cell_id after spatial hash build)
    float* d_pos_x = nullptr;
    float* d_pos_y = nullptr;
    float* d_vel_x = nullptr;
    float* d_vel_y = nullptr;
    uint8_t* d_swarm_type = nullptr;
    uint8_t* d_infected = nullptr;
    float* d_immunity = nullptr;

    // Steering / integration outputs
    float* d_force_x = nullptr;
    float* d_force_y = nullptr;
    float* d_heading = nullptr;

    // Per-boid cuRAND states (void* to avoid curand_kernel.h in host headers)
    void* d_rng_states = nullptr;

    // Stats reduction outputs (device-side single ints)
    int* d_infected_count = nullptr;
    int* d_recovered_count = nullptr;

    // Spatial hash arrays
    uint32_t* d_cell_id = nullptr;
    uint32_t* d_boid_index = nullptr;
    int* d_cell_start = nullptr;
    int* d_cell_end = nullptr;

    // Scratch arrays (sort output + reorder target + SIR mutation buffers)
    float* d_pos_x_sorted = nullptr;
    float* d_pos_y_sorted = nullptr;
    float* d_vel_x_sorted = nullptr;
    float* d_vel_y_sorted = nullptr;
    uint8_t* d_swarm_type_sorted = nullptr;
    uint8_t* d_infected_sorted = nullptr;
    float* d_immunity_sorted = nullptr;
    uint32_t* d_cell_id_sorted = nullptr;
    uint32_t* d_boid_index_sorted = nullptr;

    // CUB sort scratch
    void* d_sort_scratch = nullptr;
    size_t sort_scratch_bytes = 0;

    // Metadata
    int capacity = 0;
    int count = 0;
    int num_cells = 0;
};

void gpu_buffers_alloc(GpuBuffers& buf, int capacity, int num_cells);
void gpu_buffers_free(GpuBuffers& buf);
void gpu_buffers_upload(GpuBuffers& buf, int count,
                        const float* h_pos_x, const float* h_pos_y,
                        const float* h_vel_x, const float* h_vel_y,
                        const uint8_t* h_swarm_type,
                        const uint8_t* h_infected,
                        const float* h_immunity);
void gpu_buffers_download_positions(const GpuBuffers& buf,
                                    float* h_pos_x, float* h_pos_y, int count);
void gpu_buffers_download_infected(const GpuBuffers& buf,
                                   uint8_t* h_infected, float* h_immunity, int count);
void gpu_init_rng(GpuBuffers& buf, uint32_t seed, int count);
