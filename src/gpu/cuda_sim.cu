#include "gpu/cuda_sim.h"
#include "gpu/gpu_buffers.h"
#include "gpu/spatial_hash.h"
#include "gpu/simulation_kernels.h"
#include "gpu/sim_config_gpu.h"
#include "gpu/cuda_check.h"
#include "sim/output.h"
#include "components.h"
#include <flecs.h>
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

// Populate GPU config from CPU config
static SimConfigGpu make_gpu_config(const SimConfig& c) {
    SimConfigGpu g{};

    auto copy_swarm = [](SwarmParamsGpu& dst, const SwarmParams& src) {
        dst.cohesion_weight   = src.cohesion_weight;
        dst.alignment_weight  = src.alignment_weight;
        dst.separation_weight = src.separation_weight;
        dst.cohesion_radius   = src.cohesion_radius;
        dst.alignment_radius  = src.alignment_radius;
        dst.separation_radius = src.separation_radius;
        dst.fov               = src.fov;
        dst.noise_factor      = src.noise_factor;
        dst.max_speed         = src.max_speed;
        dst.max_force         = src.max_force;
        dst.min_speed         = src.min_speed;
    };
    copy_swarm(g.normal, c.normal);
    copy_swarm(g.doctor, c.doctor);

    g.world_w = c.world_width;
    g.world_h = c.world_height;
    g.wall_bounce = c.wall_bounce;

    g.p_infect_normal = c.p_infect_normal;
    g.p_infect_doctor = c.p_infect_doctor;
    g.p_spontaneous_infect = c.p_spontaneous_infect;
    g.p_cure = c.p_cure;
    g.r_interact_normal = c.r_interact_normal;
    g.r_interact_doctor = c.r_interact_doctor;
    g.debuff_p_cure_infected = c.debuff_p_cure_infected;
    g.debuff_r_interact_doctor_infected = c.debuff_r_interact_doctor_infected;
    g.debuff_r_interact_normal_infected = c.debuff_r_interact_normal_infected;
    g.cure_immunity_level = c.cure_immunity_level;

    g.doctor_behavior = static_cast<int>(c.doctor_behavior);
    g.doctor_seek_radius = c.doctor_seek_radius;
    g.doctor_seek_weight = c.doctor_seek_weight;

    return g;
}

void gpu_run_headless(flecs::world& world, const SimConfig& config,
                      const std::string& config_path) {
    auto wall_start = std::chrono::steady_clock::now();

    // ---- 1. Extract entity state from FLECS into host SoA arrays ----
    std::vector<float> h_pos_x, h_pos_y, h_vel_x, h_vel_y, h_immunity;
    std::vector<uint8_t> h_swarm_type, h_infected;

    auto q = world.query<const Position, const Velocity>();
    q.each([&](flecs::entity e, const Position& p, const Velocity& v) {
        h_pos_x.push_back(p.x);
        h_pos_y.push_back(p.y);
        h_vel_x.push_back(v.vx);
        h_vel_y.push_back(v.vy);
        h_swarm_type.push_back(e.has<DoctorBoid>() ? 1 : 0);
        h_infected.push_back(e.has<Infected>() ? 1 : 0);
        h_immunity.push_back(e.has<ImmunityState>()
            ? e.get<ImmunityState>().immunity_level : 0.0f);
    });

    int count = static_cast<int>(h_pos_x.size());
    if (count == 0) {
        std::fprintf(stderr, "GPU headless: no entities found\n");
        return;
    }

    // Count population breakdown
    int normal_count = 0, doctor_count = 0;
    for (int i = 0; i < count; ++i) {
        if (h_swarm_type[i] == 1) doctor_count++;
        else normal_count++;
    }

    std::printf("GPU headless: %d boids (%d normal, %d doctor)\n",
                count, normal_count, doctor_count);

    // ---- 2. Setup GPU config + grid params ----
    SimConfigGpu cfg_gpu = make_gpu_config(config);

    float cell_size = std::max(config.r_interact_normal, config.r_interact_doctor);
    GridParams grid{};
    grid.world_w = config.world_width;
    grid.world_h = config.world_height;
    grid.cell_size = cell_size;
    grid.cols = static_cast<int>(std::ceil(config.world_width / cell_size));
    grid.rows = static_cast<int>(std::ceil(config.world_height / cell_size));
    grid.total_cells = grid.cols * grid.rows;
    grid.toroidal = !config.wall_bounce;

    // Pre-compute per-swarm query radii
    float query_radius_normal = std::max({config.normal.separation_radius,
                                          config.normal.alignment_radius,
                                          config.normal.cohesion_radius});
    float query_radius_doctor = std::max({config.doctor.separation_radius,
                                          config.doctor.alignment_radius,
                                          config.doctor.cohesion_radius});
    // Extend doctor query radius for seeking
    if (config.doctor_behavior == DoctorBehavior::SeekNearest) {
        query_radius_doctor = std::max(query_radius_doctor, config.doctor_seek_radius);
    }

    // ---- 3. Allocate + upload GPU buffers ----
    GpuBuffers buf{};
    gpu_buffers_alloc(buf, count, grid.total_cells);
    gpu_buffers_upload(buf, count,
                       h_pos_x.data(), h_pos_y.data(),
                       h_vel_x.data(), h_vel_y.data(),
                       h_swarm_type.data(), h_infected.data(), h_immunity.data());
    gpu_init_rng(buf, 42, count);

    std::printf("GPU buffers allocated: %d cells (%dx%d), cell_size=%.1f\n",
                grid.total_cells, grid.cols, grid.rows, cell_size);

    // ---- 4. Open output ----
    std::string out_dir = create_output_dir(config.output_dir);
    std::printf("GPU headless mode: %.0fs (dt=%.4f), output -> %s\n",
                config.nogui_duration,
                (config.headless_dt > 0.0f) ? config.headless_dt : (1.0f / 60.0f),
                out_dir.c_str());

    // Open CSV with standard 15-column header
    std::string csv_path = (fs::path(out_dir) / "metrics.csv").string();
    FILE* csv = std::fopen(csv_path.c_str(), "w");
    if (csv) {
        std::fprintf(csv,
            "frame,time_s,infected,recovered,pct_infected,growth_rate,"
            "sick_centroid_x,sick_centroid_y,sick_alignment,"
            "normal_cohesion,normal_alignment,normal_separation,"
            "doctor_cohesion,doctor_alignment,doctor_separation\n");
    }

    // ---- 5. Simulation loop ----
    const float dt = (config.headless_dt > 0.0f) ? config.headless_dt : (1.0f / 60.0f);
    const float duration = config.nogui_duration;
    const float csv_interval = config.csv_sample_interval;
    float next_csv_write = 0.0f;

    float elapsed = 0.0f;
    int frame = 0;
    int peak_infected = 0;
    float peak_pct = 0.0f;
    int last_infected = 0;

    const float progress_step = std::max(10.0f, duration / 30.0f);
    float next_progress = 0.0f;

    while (elapsed < duration) {
        // 5a. Spatial hash build
        gpu_spatial_hash_build(buf, grid);

        // 5b. Infected centroid (for SeekCentroid mode)
        float centroid_x = 0.0f, centroid_y = 0.0f;
        if (config.doctor_behavior == DoctorBehavior::SeekCentroid) {
            gpu_infected_centroid(buf, centroid_x, centroid_y);
        }

        // 5c. Steering
        gpu_steering(buf, grid, cfg_gpu,
                     query_radius_normal, query_radius_doctor, dt,
                     centroid_x, centroid_y);

        // 5d. Integration
        gpu_integrate(buf, cfg_gpu, dt);

        // 5e. Infection (deferred): copy primary -> scratch, run kernel, swap
        CUDA_CHECK(cudaMemcpy(buf.d_infected_sorted, buf.d_infected,
                              count * sizeof(uint8_t), cudaMemcpyDeviceToDevice));
        CUDA_CHECK(cudaMemcpy(buf.d_immunity_sorted, buf.d_immunity,
                              count * sizeof(float), cudaMemcpyDeviceToDevice));
        gpu_infection(buf, grid, cfg_gpu, dt);
        std::swap(buf.d_infected, buf.d_infected_sorted);
        std::swap(buf.d_immunity, buf.d_immunity_sorted);

        // 5f. Cure (deferred): copy primary -> scratch, run kernel, swap
        CUDA_CHECK(cudaMemcpy(buf.d_infected_sorted, buf.d_infected,
                              count * sizeof(uint8_t), cudaMemcpyDeviceToDevice));
        CUDA_CHECK(cudaMemcpy(buf.d_immunity_sorted, buf.d_immunity,
                              count * sizeof(float), cudaMemcpyDeviceToDevice));
        gpu_cure(buf, grid, cfg_gpu, dt);
        std::swap(buf.d_infected, buf.d_infected_sorted);
        std::swap(buf.d_immunity, buf.d_immunity_sorted);

        // 5g. Spontaneous infection (in-place)
        gpu_spontaneous_infection(buf, cfg_gpu, dt);

        // 5h. Stats
        gpu_count_stats(buf);
        CUDA_CHECK(cudaDeviceSynchronize());

        int h_infected_count, h_recovered_count;
        CUDA_CHECK(cudaMemcpy(&h_infected_count, buf.d_infected_count,
                              sizeof(int), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&h_recovered_count, buf.d_recovered_count,
                              sizeof(int), cudaMemcpyDeviceToHost));

        float pct_infected = static_cast<float>(h_infected_count) / count;
        float growth_rate = static_cast<float>(h_infected_count - last_infected) / dt;
        last_infected = h_infected_count;

        // Track peaks
        if (h_infected_count > peak_infected) peak_infected = h_infected_count;
        if (pct_infected > peak_pct) peak_pct = pct_infected;

        // 5i. Write CSV row
        bool write_csv = (csv_interval <= 0.0f) || (elapsed >= next_csv_write);
        if (write_csv && csv) {
            std::fprintf(csv,
                "%d,%.4f,%d,%d,%.6f,%.6f,0.00,0.00,0.000000,"
                "0.000000,0.000000,0.000000,0.000000,0.000000,0.000000\n",
                frame, elapsed,
                h_infected_count, h_recovered_count,
                pct_infected, growth_rate);
            if (csv_interval > 0.0f) next_csv_write += csv_interval;
        }

        // 5j. Progress output
        if (elapsed >= next_progress) {
            std::printf("[%.1fs / %.1fs] infected: %d/%d (%.1f%%), recovered: %d\n",
                        elapsed, duration,
                        h_infected_count, count,
                        pct_infected * 100.0f,
                        h_recovered_count);
            next_progress += progress_step;
        }

        frame++;
        elapsed += dt;
    }

    if (csv) std::fclose(csv);

    // ---- 6. Write summary ----
    snapshot_config(out_dir, config_path);

    // Write summary.txt manually (matching CPU format)
    std::string summary_path = (fs::path(out_dir) / "summary.txt").string();
    FILE* sf = std::fopen(summary_path.c_str(), "w");
    if (sf) {
        std::fprintf(sf, "=== Simulation Summary ===\n\n");
        std::fprintf(sf, "Duration:           %.0f s\n", duration);
        std::fprintf(sf, "Total population:   %d\n\n", count);
        std::fprintf(sf, "--- Infection ---\n");
        std::fprintf(sf, "Peak infected:      %d / %d (%.1f%%)\n",
                     peak_infected, count, peak_pct * 100.0f);
        std::fprintf(sf, "Final infected:     %d\n", last_infected);
        std::fprintf(sf, "Total recovered:    %d\n\n", 0); // last recovered not tracked separately
        std::fprintf(sf, "--- Final Swarm Metrics ---\n");
        std::fprintf(sf, "Normal  -- cohesion: 0  alignment: 0  separation: 0\n");
        std::fprintf(sf, "Doctor  -- cohesion: 0  alignment: 0  separation: 0\n");
        std::fclose(sf);
    }

    // ---- 7. Cleanup ----
    gpu_buffers_free(buf);

    auto wall_end = std::chrono::steady_clock::now();
    double wall_secs = std::chrono::duration<double>(wall_end - wall_start).count();
    std::printf("GPU simulation complete. %d frames, %.1f wall-seconds (%.1f sim-fps)\n",
                frame, wall_secs, frame / wall_secs);
}
