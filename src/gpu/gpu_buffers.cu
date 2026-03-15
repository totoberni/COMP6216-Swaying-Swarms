#include "gpu/gpu_buffers.h"
#include "gpu/cuda_check.h"
#include <curand_kernel.h>
#include <cub/device/device_radix_sort.cuh>

// --- cuRAND init kernel ---
__global__ void initRng(curandState* states, uint32_t seed, int count) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    curand_init(seed, i, 0, &states[i]);
}

void gpu_buffers_alloc(GpuBuffers& buf, int capacity, int num_cells) {
    buf.capacity = capacity;
    buf.num_cells = num_cells;

    // Primary arrays
    CUDA_CHECK(cudaMalloc(&buf.d_pos_x, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_pos_y, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_vel_x, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_vel_y, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_swarm_type, capacity * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_infected, capacity * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_immunity, capacity * sizeof(float)));

    // Steering / integration outputs
    CUDA_CHECK(cudaMalloc(&buf.d_force_x, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_force_y, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_heading, capacity * sizeof(float)));

    // Per-boid cuRAND states
    curandState* rng_ptr = nullptr;
    CUDA_CHECK(cudaMalloc(&rng_ptr, capacity * sizeof(curandState)));
    buf.d_rng_states = rng_ptr;

    // Stats reduction outputs
    CUDA_CHECK(cudaMalloc(&buf.d_infected_count, sizeof(int)));
    CUDA_CHECK(cudaMalloc(&buf.d_recovered_count, sizeof(int)));

    // Spatial hash arrays
    CUDA_CHECK(cudaMalloc(&buf.d_cell_id, capacity * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_boid_index, capacity * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_cell_start, num_cells * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&buf.d_cell_end, num_cells * sizeof(int)));

    // Scratch arrays (sort output + reorder target + SIR mutation buffers)
    CUDA_CHECK(cudaMalloc(&buf.d_pos_x_sorted, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_pos_y_sorted, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_vel_x_sorted, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_vel_y_sorted, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_swarm_type_sorted, capacity * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_infected_sorted, capacity * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_immunity_sorted, capacity * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&buf.d_cell_id_sorted, capacity * sizeof(uint32_t)));
    CUDA_CHECK(cudaMalloc(&buf.d_boid_index_sorted, capacity * sizeof(uint32_t)));

    // CUB sort scratch — query required size
    buf.sort_scratch_bytes = 0;
    CUDA_CHECK(cub::DeviceRadixSort::SortPairs(
        nullptr, buf.sort_scratch_bytes,
        buf.d_cell_id, buf.d_cell_id_sorted,
        buf.d_boid_index, buf.d_boid_index_sorted,
        capacity));
    CUDA_CHECK(cudaMalloc(&buf.d_sort_scratch, buf.sort_scratch_bytes));
}

void gpu_buffers_free(GpuBuffers& buf) {
    cudaFree(buf.d_pos_x);        buf.d_pos_x = nullptr;
    cudaFree(buf.d_pos_y);        buf.d_pos_y = nullptr;
    cudaFree(buf.d_vel_x);        buf.d_vel_x = nullptr;
    cudaFree(buf.d_vel_y);        buf.d_vel_y = nullptr;
    cudaFree(buf.d_swarm_type);   buf.d_swarm_type = nullptr;
    cudaFree(buf.d_infected);     buf.d_infected = nullptr;
    cudaFree(buf.d_immunity);     buf.d_immunity = nullptr;

    cudaFree(buf.d_force_x);     buf.d_force_x = nullptr;
    cudaFree(buf.d_force_y);     buf.d_force_y = nullptr;
    cudaFree(buf.d_heading);     buf.d_heading = nullptr;
    cudaFree(buf.d_rng_states);  buf.d_rng_states = nullptr;
    cudaFree(buf.d_infected_count);   buf.d_infected_count = nullptr;
    cudaFree(buf.d_recovered_count);  buf.d_recovered_count = nullptr;

    cudaFree(buf.d_cell_id);      buf.d_cell_id = nullptr;
    cudaFree(buf.d_boid_index);   buf.d_boid_index = nullptr;
    cudaFree(buf.d_cell_start);   buf.d_cell_start = nullptr;
    cudaFree(buf.d_cell_end);     buf.d_cell_end = nullptr;

    cudaFree(buf.d_pos_x_sorted);      buf.d_pos_x_sorted = nullptr;
    cudaFree(buf.d_pos_y_sorted);      buf.d_pos_y_sorted = nullptr;
    cudaFree(buf.d_vel_x_sorted);      buf.d_vel_x_sorted = nullptr;
    cudaFree(buf.d_vel_y_sorted);      buf.d_vel_y_sorted = nullptr;
    cudaFree(buf.d_swarm_type_sorted); buf.d_swarm_type_sorted = nullptr;
    cudaFree(buf.d_infected_sorted);   buf.d_infected_sorted = nullptr;
    cudaFree(buf.d_immunity_sorted);   buf.d_immunity_sorted = nullptr;
    cudaFree(buf.d_cell_id_sorted);    buf.d_cell_id_sorted = nullptr;
    cudaFree(buf.d_boid_index_sorted); buf.d_boid_index_sorted = nullptr;

    cudaFree(buf.d_sort_scratch);      buf.d_sort_scratch = nullptr;
    buf.sort_scratch_bytes = 0;
    buf.capacity = 0;
    buf.count = 0;
    buf.num_cells = 0;
}

void gpu_buffers_upload(GpuBuffers& buf, int count,
                        const float* h_pos_x, const float* h_pos_y,
                        const float* h_vel_x, const float* h_vel_y,
                        const uint8_t* h_swarm_type,
                        const uint8_t* h_infected,
                        const float* h_immunity) {
    buf.count = count;
    CUDA_CHECK(cudaMemcpy(buf.d_pos_x, h_pos_x, count * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_pos_y, h_pos_y, count * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_vel_x, h_vel_x, count * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_vel_y, h_vel_y, count * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_swarm_type, h_swarm_type, count * sizeof(uint8_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_infected, h_infected, count * sizeof(uint8_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(buf.d_immunity, h_immunity, count * sizeof(float), cudaMemcpyHostToDevice));
}

void gpu_buffers_download_positions(const GpuBuffers& buf,
                                    float* h_pos_x, float* h_pos_y, int count) {
    CUDA_CHECK(cudaMemcpy(h_pos_x, buf.d_pos_x, count * sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(h_pos_y, buf.d_pos_y, count * sizeof(float), cudaMemcpyDeviceToHost));
}

void gpu_buffers_download_infected(const GpuBuffers& buf,
                                   uint8_t* h_infected, float* h_immunity, int count) {
    CUDA_CHECK(cudaMemcpy(h_infected, buf.d_infected, count * sizeof(uint8_t), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(h_immunity, buf.d_immunity, count * sizeof(float), cudaMemcpyDeviceToHost));
}

void gpu_init_rng(GpuBuffers& buf, uint32_t seed, int count) {
    const int block_size = 256;
    const int grid_size = (count + block_size - 1) / block_size;
    initRng<<<grid_size, block_size>>>(static_cast<curandState*>(buf.d_rng_states), seed, count);
    CUDA_CHECK(cudaDeviceSynchronize());
}
