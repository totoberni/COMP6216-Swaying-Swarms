#pragma once

#include "gpu/gpu_buffers.h"
#include "gpu/spatial_hash.h"
#include "gpu/sim_config_gpu.h"

// Steering: boid forces (separation, alignment, cohesion) + doctor seeking + noise
void gpu_steering(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg,
                  float query_radius_normal, float query_radius_doctor, float dt,
                  float centroid_x, float centroid_y);

// Integration: vel += force, pos update, boundary handling, heading
void gpu_integrate(GpuBuffers& buf, const SimConfigGpu& cfg, float dt);

// Infection: neighbor-based stochastic transmission (deferred write)
// Caller must copy primary->scratch BEFORE, swap AFTER
void gpu_infection(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg, float dt);

// Cure: doctor-based stochastic cure (deferred write)
// Caller must copy primary->scratch BEFORE, swap AFTER
void gpu_cure(GpuBuffers& buf, const GridParams& grid, const SimConfigGpu& cfg, float dt);

// Spontaneous infection: per-boid probability, in-place
void gpu_spontaneous_infection(GpuBuffers& buf, const SimConfigGpu& cfg, float dt);

// Stats: count infected + recovered via atomicAdd reduction
// Writes to buf.d_infected_count and buf.d_recovered_count
void gpu_count_stats(GpuBuffers& buf);

// Infected centroid reduction (for SeekCentroid mode)
// Returns centroid via host pointers
void gpu_infected_centroid(const GpuBuffers& buf, float& cx, float& cy);
