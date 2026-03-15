#include "gpu/simulation_kernels.h"
#include "gpu/cuda_check.h"
#include <curand_kernel.h>
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static constexpr int BLOCK_SIZE = 256;

// ============================================================
// Device helpers
// ============================================================

// Shortest signed displacement on periodic axis [0, period)
__device__ inline float d_torus_diff(float from, float to, float period) {
    float d = to - from;
    float half = period * 0.5f;
    if (d > half) d -= period;
    if (d < -half) d += period;
    return d;
}

// Displacement from (ax,ay) to (bx,by) — toroidal or Euclidean
__device__ inline void d_displacement(float ax, float ay, float bx, float by,
                                      bool toroidal, float ww, float wh,
                                      float& dx, float& dy) {
    if (toroidal) {
        dx = d_torus_diff(ax, bx, ww);
        dy = d_torus_diff(ay, by, wh);
    } else {
        dx = bx - ax;
        dy = by - ay;
    }
}

// Reynolds steer_toward: normalize -> scale to max_speed -> subtract current -> clamp to max_force
__device__ inline void d_steer_toward(float dir_x, float dir_y, float max_speed,
                                      float cur_vx, float cur_vy, float max_force,
                                      float& out_x, float& out_y) {
    float mag = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (mag < 0.001f) { out_x = 0.0f; out_y = 0.0f; return; }
    float scale = max_speed / mag;
    float desired_x = dir_x * scale;
    float desired_y = dir_y * scale;
    float steer_x = desired_x - cur_vx;
    float steer_y = desired_y - cur_vy;
    float steer_mag = sqrtf(steer_x * steer_x + steer_y * steer_y);
    if (steer_mag > max_force) {
        float clamp = max_force / steer_mag;
        steer_x *= clamp;
        steer_y *= clamp;
    }
    out_x = steer_x;
    out_y = steer_y;
}

// ============================================================
// Steering Kernel
// ============================================================

__global__ void steeringKernel(
    const float* __restrict__ pos_x, const float* __restrict__ pos_y,
    const float* __restrict__ vel_x, const float* __restrict__ vel_y,
    const uint8_t* __restrict__ swarm_type, const uint8_t* __restrict__ infected,
    const int* __restrict__ cell_start, const int* __restrict__ cell_end,
    float* __restrict__ force_x, float* __restrict__ force_y,
    curandState* __restrict__ rng_states,
    SimConfigGpu cfg, GridParams grid,
    float query_radius_normal, float query_radius_doctor,
    float dt,
    int doctor_behavior, float centroid_x, float centroid_y,
    int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    float px = pos_x[i], py = pos_y[i];
    float vx = vel_x[i], vy = vel_y[i];
    int my_swarm = swarm_type[i];

    // Select per-swarm params
    const SwarmParamsGpu& params = (my_swarm == 1) ? cfg.doctor : cfg.normal;
    float query_radius = (my_swarm == 1) ? query_radius_doctor : query_radius_normal;

    float sep_r_sq = params.separation_radius * params.separation_radius;
    float ali_r_sq = params.alignment_radius * params.alignment_radius;
    float coh_r_sq = params.cohesion_radius * params.cohesion_radius;
    float query_r_sq = query_radius * query_radius;

    bool toroidal = !cfg.wall_bounce;

    // FOV precomputation (replicating CPU spatial_grid.cpp query_neighbors_fov)
    float cos_fov = cosf(params.fov);
    float vel_mag_sq = vx * vx + vy * vy;
    float cos_fov_sq = cos_fov * cos_fov;
    float fov_threshold = cos_fov_sq * vel_mag_sq;
    bool wide_fov = (cos_fov < 0.0f);

    // Accumulators
    float sep_x = 0.0f, sep_y = 0.0f; int sep_count = 0;
    float ali_x = 0.0f, ali_y = 0.0f; int ali_count = 0;
    float coh_x = 0.0f, coh_y = 0.0f; int coh_count = 0;

    // Doctor SeekNearest: track nearest infected during iteration
    float best_dist_sq = cfg.doctor_seek_radius * cfg.doctor_seek_radius + 1.0f;
    float nearest_x = 0.0f, nearest_y = 0.0f;
    bool found_nearest = false;

    // Compute cell coordinates
    int col = static_cast<int>(px / grid.cell_size);
    int row_i = static_cast<int>(py / grid.cell_size);
    col = max(0, min(col, grid.cols - 1));
    row_i = max(0, min(row_i, grid.rows - 1));

    int cell_range = static_cast<int>(ceilf(query_radius / grid.cell_size));

    // Iterate surrounding cells
    for (int dy = -cell_range; dy <= cell_range; ++dy) {
        for (int dx = -cell_range; dx <= cell_range; ++dx) {
            int check_col = col + dx;
            int check_row = row_i + dy;

            if (grid.toroidal) {
                check_col = ((check_col % grid.cols) + grid.cols) % grid.cols;
                check_row = ((check_row % grid.rows) + grid.rows) % grid.rows;
            } else {
                if (check_col < 0 || check_col >= grid.cols ||
                    check_row < 0 || check_row >= grid.rows) continue;
            }

            int cid = check_col + grid.cols * check_row;
            if (cid < 0 || cid >= grid.total_cells) continue;
            if (cell_start[cid] < 0) continue;

            for (int j = cell_start[cid]; j < cell_end[cid]; ++j) {
                if (j == i) continue;

                float nx = pos_x[j], ny = pos_y[j];
                float diff_x, diff_y;
                d_displacement(px, py, nx, ny, toroidal, cfg.world_w, cfg.world_h, diff_x, diff_y);

                float dist_sq = diff_x * diff_x + diff_y * diff_y;
                if (dist_sq < 0.000001f) continue;
                if (dist_sq > query_r_sq) continue;

                // FOV check
                if (vel_mag_sq >= 0.00000001f) {
                    float dot = vx * diff_x + vy * diff_y;
                    float dot_sq = dot * dot;
                    float thresh = fov_threshold * dist_sq;
                    bool in_fov = wide_fov
                        ? (dot >= 0.0f || dot_sq <= thresh)
                        : (dot >= 0.0f && dot_sq >= thresh);
                    if (!in_fov) continue;
                }

                int ne_swarm = swarm_type[j];

                // Separation: ALL swarms (cross-swarm repulsion)
                if (dist_sq < sep_r_sq) {
                    // Displacement from NEIGHBOR to SELF (repulsive vector)
                    float sdx, sdy;
                    d_displacement(nx, ny, px, py, toroidal, cfg.world_w, cfg.world_h, sdx, sdy);
                    sep_x += sdx / dist_sq;
                    sep_y += sdy / dist_sq;
                    sep_count++;
                }

                // Alignment: same swarm only
                if (dist_sq < ali_r_sq && ne_swarm == my_swarm) {
                    ali_x += vel_x[j];
                    ali_y += vel_y[j];
                    ali_count++;
                }

                // Cohesion: same swarm only, displacement from SELF to NEIGHBOR
                if (dist_sq < coh_r_sq && ne_swarm == my_swarm) {
                    coh_x += diff_x;
                    coh_y += diff_y;
                    coh_count++;
                }

                // Doctor SeekNearest: track nearest infected
                if (doctor_behavior == 1 && my_swarm == 1 && infected[j]) {
                    if (dist_sq < best_dist_sq) {
                        best_dist_sq = dist_sq;
                        nearest_x = nx;
                        nearest_y = ny;
                        found_nearest = true;
                    }
                }
            }
        }
    }

    // Compute steering force
    float fx = 0.0f, fy = 0.0f;
    float sx, sy;

    if (sep_count > 0) {
        float inv = 1.0f / sep_count;
        d_steer_toward(sep_x * inv, sep_y * inv, params.max_speed, vx, vy, params.max_force, sx, sy);
        fx += sx * params.separation_weight;
        fy += sy * params.separation_weight;
    }

    if (ali_count > 0) {
        float inv = 1.0f / ali_count;
        d_steer_toward(ali_x * inv, ali_y * inv, params.max_speed, vx, vy, params.max_force, sx, sy);
        fx += sx * params.alignment_weight;
        fy += sy * params.alignment_weight;
    }

    if (coh_count > 0) {
        float inv = 1.0f / coh_count;
        d_steer_toward(coh_x * inv, coh_y * inv, params.max_speed, vx, vy, params.max_force, sx, sy);
        fx += sx * params.cohesion_weight;
        fy += sy * params.cohesion_weight;
    }

    // Noise injection (per-swarm noise_factor)
    if (params.noise_factor > 0.0f) {
        float angle = curand_uniform(&rng_states[i]) * 2.0f * M_PI;
        fx += cosf(angle) * params.noise_factor;
        fy += sinf(angle) * params.noise_factor;
    }

    // Doctor seeking
    if (my_swarm == 1) {
        if (doctor_behavior == 1 && found_nearest) {
            // SeekNearest: Reynolds seek toward nearest infected
            float tdx, tdy;
            d_displacement(px, py, nearest_x, nearest_y, toroidal, cfg.world_w, cfg.world_h, tdx, tdy);
            float tmag = sqrtf(tdx * tdx + tdy * tdy);
            if (tmag > 0.001f) {
                float desired_x = tdx * cfg.doctor.max_speed / tmag;
                float desired_y = tdy * cfg.doctor.max_speed / tmag;
                float seek_x = (desired_x - vx) * cfg.doctor_seek_weight;
                float seek_y = (desired_y - vy) * cfg.doctor_seek_weight;
                fx += seek_x;
                fy += seek_y;
            }
        } else if (doctor_behavior == 2) {
            // SeekCentroid: seek toward pre-computed infected centroid
            float tdx, tdy;
            d_displacement(px, py, centroid_x, centroid_y, toroidal, cfg.world_w, cfg.world_h, tdx, tdy);
            float tmag = sqrtf(tdx * tdx + tdy * tdy);
            if (tmag > 0.001f) {
                float desired_x = tdx * cfg.doctor.max_speed / tmag;
                float desired_y = tdy * cfg.doctor.max_speed / tmag;
                float seek_x = (desired_x - vx) * cfg.doctor_seek_weight;
                float seek_y = (desired_y - vy) * cfg.doctor_seek_weight;
                fx += seek_x;
                fy += seek_y;
            }
        }
    }

    force_x[i] = fx;
    force_y[i] = fy;
}

// ============================================================
// Integration Kernel
// ============================================================

__global__ void integrateKernel(
    float* __restrict__ pos_x, float* __restrict__ pos_y,
    float* __restrict__ vel_x, float* __restrict__ vel_y,
    const uint8_t* __restrict__ swarm_type,
    const float* __restrict__ force_x_in, const float* __restrict__ force_y_in,
    float* __restrict__ heading,
    SimConfigGpu cfg, float dt, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    int my_swarm = swarm_type[i];
    const SwarmParamsGpu& params = (my_swarm == 1) ? cfg.doctor : cfg.normal;

    // Apply force
    float vx_new = vel_x[i] + force_x_in[i] * dt;
    float vy_new = vel_y[i] + force_y_in[i] * dt;

    // Clamp to max_speed
    float speed = sqrtf(vx_new * vx_new + vy_new * vy_new);
    if (speed > params.max_speed) {
        float s = params.max_speed / speed;
        vx_new *= s;
        vy_new *= s;
        speed = params.max_speed;
    }

    // Update position
    float px = pos_x[i] + vx_new * dt;
    float py = pos_y[i] + vy_new * dt;

    // Boundary handling
    if (cfg.wall_bounce) {
        if (px < 0.0f) { px = -px; vx_new = -vx_new; }
        if (px >= cfg.world_w) { px = 2.0f * cfg.world_w - px; vx_new = -vx_new; }
        if (py < 0.0f) { py = -py; vy_new = -vy_new; }
        if (py >= cfg.world_h) { py = 2.0f * cfg.world_h - py; vy_new = -vy_new; }
    } else {
        if (px < 0.0f) px += cfg.world_w;
        if (px >= cfg.world_w) px -= cfg.world_w;
        if (py < 0.0f) py += cfg.world_h;
        if (py >= cfg.world_h) py -= cfg.world_h;
    }

    // Recompute speed after boundary changes
    speed = sqrtf(vx_new * vx_new + vy_new * vy_new);

    // Enforce min_speed
    if (speed > 0.001f && speed < params.min_speed && params.min_speed <= params.max_speed) {
        float s = params.min_speed / speed;
        vx_new *= s;
        vy_new *= s;
        speed = params.min_speed;
    }

    // Update heading
    if (speed > 0.01f) {
        heading[i] = atan2f(vy_new, vx_new);
    }

    pos_x[i] = px;
    pos_y[i] = py;
    vel_x[i] = vx_new;
    vel_y[i] = vy_new;
}

// ============================================================
// Infection Kernel (deferred write — reads d_infected, writes d_infected_sorted)
// ============================================================

__global__ void infectionKernel(
    const float* __restrict__ pos_x, const float* __restrict__ pos_y,
    const uint8_t* __restrict__ swarm_type,
    const uint8_t* __restrict__ infected_read,
    const float* __restrict__ immunity_read,
    uint8_t* __restrict__ infected_write,
    float* __restrict__ immunity_write,
    const int* __restrict__ cell_start, const int* __restrict__ cell_end,
    curandState* __restrict__ rng_states,
    SimConfigGpu cfg, GridParams grid, float dt, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    // Only infected boids are spreaders
    if (!infected_read[i]) return;

    float px = pos_x[i], py = pos_y[i];
    bool is_doctor = (swarm_type[i] == 1);
    bool toroidal = !cfg.wall_bounce;

    float effective_r, p_infect;
    if (is_doctor) {
        effective_r = cfg.r_interact_doctor * cfg.debuff_r_interact_doctor_infected;
        p_infect = cfg.p_infect_doctor;
    } else {
        effective_r = cfg.r_interact_normal * cfg.debuff_r_interact_normal_infected;
        p_infect = cfg.p_infect_normal;
    }
    float r_sq = effective_r * effective_r;

    int col = static_cast<int>(px / grid.cell_size);
    int row_i = static_cast<int>(py / grid.cell_size);
    col = max(0, min(col, grid.cols - 1));
    row_i = max(0, min(row_i, grid.rows - 1));
    int cell_range = static_cast<int>(ceilf(effective_r / grid.cell_size));

    for (int dy = -cell_range; dy <= cell_range; ++dy) {
        for (int dx = -cell_range; dx <= cell_range; ++dx) {
            int cc = col + dx, cr = row_i + dy;
            if (grid.toroidal) {
                cc = ((cc % grid.cols) + grid.cols) % grid.cols;
                cr = ((cr % grid.rows) + grid.rows) % grid.rows;
            } else {
                if (cc < 0 || cc >= grid.cols || cr < 0 || cr >= grid.rows) continue;
            }

            int cid = cc + grid.cols * cr;
            if (cid < 0 || cid >= grid.total_cells) continue;
            if (cell_start[cid] < 0) continue;

            for (int j = cell_start[cid]; j < cell_end[cid]; ++j) {
                if (j == i) continue;
                if (infected_read[j]) continue;  // already infected

                float diff_x, diff_y;
                d_displacement(px, py, pos_x[j], pos_y[j], toroidal, cfg.world_w, cfg.world_h, diff_x, diff_y);
                float dist_sq = diff_x * diff_x + diff_y * diff_y;
                if (dist_sq > r_sq) continue;

                // Apply immunity reduction
                float eff_p = p_infect * (1.0f - immunity_read[j]);

                // dt-scale: p_per_frame = 1 - pow(1 - p, dt)
                float p_frame = 1.0f - powf(1.0f - eff_p, dt);

                if (curand_uniform(&rng_states[i]) < p_frame) {
                    infected_write[j] = 1;
                    immunity_write[j] = 0.0f;
                }
            }
        }
    }
}

// ============================================================
// Cure Kernel (deferred write)
// ============================================================

__global__ void cureKernel(
    const float* __restrict__ pos_x, const float* __restrict__ pos_y,
    const uint8_t* __restrict__ swarm_type,
    const uint8_t* __restrict__ infected_read,
    uint8_t* __restrict__ infected_write,
    float* __restrict__ immunity_write,
    const int* __restrict__ cell_start, const int* __restrict__ cell_end,
    curandState* __restrict__ rng_states,
    SimConfigGpu cfg, GridParams grid, float dt, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    // Only doctors can cure
    if (swarm_type[i] != 1) return;

    float px = pos_x[i], py = pos_y[i];
    bool doctor_infected = (infected_read[i] != 0);
    bool toroidal = !cfg.wall_bounce;

    float effective_r = cfg.r_interact_doctor;
    if (doctor_infected) effective_r *= cfg.debuff_r_interact_doctor_infected;
    float r_sq = effective_r * effective_r;

    float effective_p_cure = cfg.p_cure;
    if (doctor_infected) effective_p_cure *= cfg.debuff_p_cure_infected;

    float p_frame = 1.0f - powf(1.0f - effective_p_cure, dt);

    int col = static_cast<int>(px / grid.cell_size);
    int row_i = static_cast<int>(py / grid.cell_size);
    col = max(0, min(col, grid.cols - 1));
    row_i = max(0, min(row_i, grid.rows - 1));
    int cell_range = static_cast<int>(ceilf(effective_r / grid.cell_size));

    for (int dy = -cell_range; dy <= cell_range; ++dy) {
        for (int dx = -cell_range; dx <= cell_range; ++dx) {
            int cc = col + dx, cr = row_i + dy;
            if (grid.toroidal) {
                cc = ((cc % grid.cols) + grid.cols) % grid.cols;
                cr = ((cr % grid.rows) + grid.rows) % grid.rows;
            } else {
                if (cc < 0 || cc >= grid.cols || cr < 0 || cr >= grid.rows) continue;
            }

            int cid = cc + grid.cols * cr;
            if (cid < 0 || cid >= grid.total_cells) continue;
            if (cell_start[cid] < 0) continue;

            for (int j = cell_start[cid]; j < cell_end[cid]; ++j) {
                if (j == i) continue;
                if (!infected_read[j]) continue;  // not infected, skip

                if (curand_uniform(&rng_states[i]) < p_frame) {
                    infected_write[j] = 0;
                    immunity_write[j] = cfg.cure_immunity_level;
                }
            }
        }
    }
}

// ============================================================
// Spontaneous Infection Kernel (in-place — safe: only sets 0->1 per own boid)
// ============================================================

__global__ void spontaneousInfectionKernel(
    uint8_t* __restrict__ infected,
    float* __restrict__ immunity,
    curandState* __restrict__ rng_states,
    float p_spontaneous, float dt, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    if (infected[i]) return;

    float p_frame = p_spontaneous * dt;
    if (curand_uniform(&rng_states[i]) < p_frame) {
        infected[i] = 1;
        immunity[i] = 0.0f;
    }
}

// ============================================================
// Stats Reduction Kernel
// ============================================================

__global__ void countStatsKernel(
    const uint8_t* __restrict__ infected,
    const float* __restrict__ immunity,
    int* __restrict__ d_infected_count,
    int* __restrict__ d_recovered_count,
    int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;

    if (infected[i]) {
        atomicAdd(d_infected_count, 1);
    } else if (immunity[i] > 0.0f) {
        atomicAdd(d_recovered_count, 1);
    }
}

// ============================================================
// Infected Centroid Reduction
// ============================================================

__global__ void infectedCentroidKernel(
    const float* __restrict__ pos_x, const float* __restrict__ pos_y,
    const uint8_t* __restrict__ infected,
    float* __restrict__ sum_x, float* __restrict__ sum_y,
    int* __restrict__ d_count, int count)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count) return;
    if (!infected[i]) return;
    atomicAdd(sum_x, pos_x[i]);
    atomicAdd(sum_y, pos_y[i]);
    atomicAdd(d_count, 1);
}

// ============================================================
// Host launcher functions
// ============================================================

void gpu_steering(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg,
                  float query_radius_normal, float query_radius_doctor, float dt,
                  float centroid_x, float centroid_y) {
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    curandState* rng = static_cast<curandState*>(buf.d_rng_states);
    steeringKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_pos_x, buf.d_pos_y, buf.d_vel_x, buf.d_vel_y,
        buf.d_swarm_type, buf.d_infected,
        buf.d_cell_start, buf.d_cell_end,
        buf.d_force_x, buf.d_force_y,
        rng,
        cfg, grid, query_radius_normal, query_radius_doctor, dt,
        cfg.doctor_behavior, centroid_x, centroid_y,
        buf.count);
}

void gpu_integrate(GpuBuffers& buf, const SimConfigGpu& cfg, float dt) {
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    integrateKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_pos_x, buf.d_pos_y, buf.d_vel_x, buf.d_vel_y,
        buf.d_swarm_type,
        buf.d_force_x, buf.d_force_y,
        buf.d_heading,
        cfg, dt, buf.count);
}

void gpu_infection(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg, float dt) {
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    curandState* rng = static_cast<curandState*>(buf.d_rng_states);
    infectionKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_pos_x, buf.d_pos_y, buf.d_swarm_type,
        buf.d_infected, buf.d_immunity,
        buf.d_infected_sorted, buf.d_immunity_sorted,
        buf.d_cell_start, buf.d_cell_end,
        rng,
        cfg, grid, dt, buf.count);
}

void gpu_cure(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg, float dt) {
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    curandState* rng = static_cast<curandState*>(buf.d_rng_states);
    cureKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_pos_x, buf.d_pos_y, buf.d_swarm_type,
        buf.d_infected,
        buf.d_infected_sorted, buf.d_immunity_sorted,
        buf.d_cell_start, buf.d_cell_end,
        rng,
        cfg, grid, dt, buf.count);
}

void gpu_spontaneous_infection(GpuBuffers& buf, const SimConfigGpu& cfg, float dt) {
    if (cfg.p_spontaneous_infect <= 0.0f) return;
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    curandState* rng = static_cast<curandState*>(buf.d_rng_states);
    spontaneousInfectionKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_infected, buf.d_immunity, rng,
        cfg.p_spontaneous_infect, dt, buf.count);
}

void gpu_count_stats(GpuBuffers& buf) {
    CUDA_CHECK(cudaMemset(buf.d_infected_count, 0, sizeof(int)));
    CUDA_CHECK(cudaMemset(buf.d_recovered_count, 0, sizeof(int)));
    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    countStatsKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_infected, buf.d_immunity,
        buf.d_infected_count, buf.d_recovered_count,
        buf.count);
}

void gpu_infected_centroid(const GpuBuffers& buf, float& cx, float& cy) {
    // Allocate temp device vars
    float* d_sum_x; float* d_sum_y; int* d_cnt;
    CUDA_CHECK(cudaMalloc(&d_sum_x, sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_sum_y, sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_cnt, sizeof(int)));
    CUDA_CHECK(cudaMemset(d_sum_x, 0, sizeof(float)));
    CUDA_CHECK(cudaMemset(d_sum_y, 0, sizeof(float)));
    CUDA_CHECK(cudaMemset(d_cnt, 0, sizeof(int)));

    int grid_size = (buf.count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    infectedCentroidKernel<<<grid_size, BLOCK_SIZE>>>(
        buf.d_pos_x, buf.d_pos_y, buf.d_infected,
        d_sum_x, d_sum_y, d_cnt, buf.count);
    CUDA_CHECK(cudaDeviceSynchronize());

    float h_sx, h_sy; int h_cnt;
    CUDA_CHECK(cudaMemcpy(&h_sx, d_sum_x, sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(&h_sy, d_sum_y, sizeof(float), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(&h_cnt, d_cnt, sizeof(int), cudaMemcpyDeviceToHost));

    cudaFree(d_sum_x);
    cudaFree(d_sum_y);
    cudaFree(d_cnt);

    if (h_cnt > 0) {
        cx = h_sx / h_cnt;
        cy = h_sy / h_cnt;
    } else {
        cx = 0.0f;
        cy = 0.0f;
    }
}
