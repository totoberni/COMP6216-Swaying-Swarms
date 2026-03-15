#include "gpu/spatial_hash.h"
#include "gpu/cuda_check.h"
#include <cub/device/device_radix_sort.cuh>
#include <algorithm>

// --- Kernel 1: compute cell hash + identity permutation ---
__global__ void calcHash(
    const float* __restrict__ pos_x,
    const float* __restrict__ pos_y,
    uint32_t* __restrict__ cell_id,
    uint32_t* __restrict__ boid_index,
    float cell_size, int cols, int rows, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    int col = static_cast<int>(pos_x[i] / cell_size);
    int row = static_cast<int>(pos_y[i] / cell_size);
    col = max(0, min(col, cols - 1));
    row = max(0, min(row, rows - 1));

    cell_id[i] = static_cast<uint32_t>(col + cols * row);
    boid_index[i] = static_cast<uint32_t>(i);
}

// --- Kernel 2: find cell start/end boundaries in sorted array ---
__global__ void findCellStart(
    const uint32_t* __restrict__ sorted_cell_id,
    int* __restrict__ cell_start,
    int* __restrict__ cell_end,
    int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    uint32_t cid = sorted_cell_id[i];

    if (i == 0 || cid != sorted_cell_id[i - 1]) {
        cell_start[cid] = i;
    }
    if (i == count - 1 || cid != sorted_cell_id[i + 1]) {
        cell_end[cid] = i + 1;
    }
}

// --- Kernel 3: scatter SoA arrays into cell-sorted order ---
__global__ void reorderBoids(
    const uint32_t* __restrict__ sorted_boid_index,
    const float* __restrict__ src_pos_x, const float* __restrict__ src_pos_y,
    const float* __restrict__ src_vel_x, const float* __restrict__ src_vel_y,
    const uint8_t* __restrict__ src_swarm_type,
    const uint8_t* __restrict__ src_infected,
    const float* __restrict__ src_immunity,
    float* __restrict__ dst_pos_x, float* __restrict__ dst_pos_y,
    float* __restrict__ dst_vel_x, float* __restrict__ dst_vel_y,
    uint8_t* __restrict__ dst_swarm_type,
    uint8_t* __restrict__ dst_infected,
    float* __restrict__ dst_immunity,
    int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    uint32_t src_idx = sorted_boid_index[i];
    dst_pos_x[i] = src_pos_x[src_idx];
    dst_pos_y[i] = src_pos_y[src_idx];
    dst_vel_x[i] = src_vel_x[src_idx];
    dst_vel_y[i] = src_vel_y[src_idx];
    dst_swarm_type[i] = src_swarm_type[src_idx];
    dst_infected[i] = src_infected[src_idx];
    dst_immunity[i] = src_immunity[src_idx];
}

void gpu_spatial_hash_build(GpuBuffers& buf, const GridParams& grid) {
    if (buf.count == 0) return;

    const int block_size = 256;
    const int grid_size = (buf.count + block_size - 1) / block_size;

    // 1. Pre-fill cell_start with -1 (0xFF bytes -> 0xFFFFFFFF = -1 two's complement)
    CUDA_CHECK(cudaMemset(buf.d_cell_start, 0xFF, grid.total_cells * sizeof(int)));
    CUDA_CHECK(cudaMemset(buf.d_cell_end, 0x00, grid.total_cells * sizeof(int)));

    // 2. Compute cell hash + identity permutation
    calcHash<<<grid_size, block_size>>>(
        buf.d_pos_x, buf.d_pos_y,
        buf.d_cell_id, buf.d_boid_index,
        grid.cell_size, grid.cols, grid.rows, buf.count);

    // 3. CUB radix sort (cell_id, boid_index) -> sorted variants
    CUDA_CHECK(cub::DeviceRadixSort::SortPairs(
        buf.d_sort_scratch, buf.sort_scratch_bytes,
        buf.d_cell_id, buf.d_cell_id_sorted,
        buf.d_boid_index, buf.d_boid_index_sorted,
        buf.count));

    // 4. Find cell boundaries in sorted array
    findCellStart<<<grid_size, block_size>>>(
        buf.d_cell_id_sorted, buf.d_cell_start, buf.d_cell_end, buf.count);

    // 5. Reorder all SoA arrays into cell-sorted order
    reorderBoids<<<grid_size, block_size>>>(
        buf.d_boid_index_sorted,
        buf.d_pos_x, buf.d_pos_y,
        buf.d_vel_x, buf.d_vel_y,
        buf.d_swarm_type, buf.d_infected, buf.d_immunity,
        buf.d_pos_x_sorted, buf.d_pos_y_sorted,
        buf.d_vel_x_sorted, buf.d_vel_y_sorted,
        buf.d_swarm_type_sorted, buf.d_infected_sorted, buf.d_immunity_sorted,
        buf.count);

    // 6. Pointer swap: sorted becomes primary (zero-copy)
    std::swap(buf.d_pos_x, buf.d_pos_x_sorted);
    std::swap(buf.d_pos_y, buf.d_pos_y_sorted);
    std::swap(buf.d_vel_x, buf.d_vel_x_sorted);
    std::swap(buf.d_vel_y, buf.d_vel_y_sorted);
    std::swap(buf.d_swarm_type, buf.d_swarm_type_sorted);
    std::swap(buf.d_infected, buf.d_infected_sorted);
    std::swap(buf.d_immunity, buf.d_immunity_sorted);
    std::swap(buf.d_cell_id, buf.d_cell_id_sorted);
    std::swap(buf.d_boid_index, buf.d_boid_index_sorted);

    // 7. Synchronize
    CUDA_CHECK(cudaDeviceSynchronize());
}
