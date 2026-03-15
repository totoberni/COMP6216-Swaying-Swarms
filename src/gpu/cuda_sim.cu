#include "gpu/cuda_sim.h"
#include "gpu/gpu_buffers.h"
#include "gpu/spatial_hash.h"
#include "gpu/simulation_kernels.h"
#include "gpu/sim_config_gpu.h"
#include "gpu/cuda_check.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// --- Output helpers (avoid including sim/output.h which pulls components.h) ---

static std::string gpu_create_output_dir(const char* base) {
    fs::path base_path(base);
    if (!fs::exists(base_path)) fs::create_directories(base_path);

    int max_n = -1;
    for (const auto& entry : fs::directory_iterator(base_path)) {
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.size() > 3 && name.substr(0, 3) == "out") {
            try { int n = std::stoi(name.substr(3)); if (n > max_n) max_n = n; }
            catch (...) {}
        }
    }
    fs::path new_dir = base_path / ("out" + std::to_string(max_n + 1));
    fs::create_directories(new_dir);
    return new_dir.string();
}

static void gpu_snapshot_config(const std::string& dir, const std::string& config_path) {
    fs::path src(config_path);
    if (!fs::exists(src)) return;
    fs::copy_file(src, fs::path(dir) / "config_used.ini", fs::copy_options::overwrite_existing);
}

void gpu_run_headless(
    int count, int normal_count, int doctor_count,
    float* h_pos_x, float* h_pos_y,
    float* h_vel_x, float* h_vel_y,
    uint8_t* h_swarm_type, uint8_t* h_infected, float* h_immunity,
    const SimConfigGpu& cfg,
    float duration, float dt, float csv_interval,
    const char* output_dir, const std::string& config_path)
{
    auto wall_start = std::chrono::steady_clock::now();

    if (count == 0) {
        std::fprintf(stderr, "GPU headless: no entities\n");
        return;
    }

    std::printf("GPU headless: %d boids (%d normal, %d doctor)\n",
                count, normal_count, doctor_count);

    // ---- Setup grid params ----
    float cell_size = std::max(cfg.r_interact_normal, cfg.r_interact_doctor);
    GridParams grid{};
    grid.world_w = cfg.world_w;
    grid.world_h = cfg.world_h;
    grid.cell_size = cell_size;
    grid.cols = static_cast<int>(std::ceil(cfg.world_w / cell_size));
    grid.rows = static_cast<int>(std::ceil(cfg.world_h / cell_size));
    grid.total_cells = grid.cols * grid.rows;
    grid.toroidal = !cfg.wall_bounce;

    // Per-swarm query radii
    float qr_normal = std::max({cfg.normal.separation_radius,
                                cfg.normal.alignment_radius,
                                cfg.normal.cohesion_radius});
    float qr_doctor = std::max({cfg.doctor.separation_radius,
                                cfg.doctor.alignment_radius,
                                cfg.doctor.cohesion_radius});
    if (cfg.doctor_behavior == 1) {  // SeekNearest
        qr_doctor = std::max(qr_doctor, cfg.doctor_seek_radius);
    }

    // ---- Allocate + upload GPU buffers ----
    GpuBuffers buf{};
    gpu_buffers_alloc(buf, count, grid.total_cells);
    gpu_buffers_upload(buf, count, h_pos_x, h_pos_y, h_vel_x, h_vel_y,
                       h_swarm_type, h_infected, h_immunity);
    gpu_init_rng(buf, 42, count);

    std::printf("GPU buffers allocated: %d cells (%dx%d), cell_size=%.1f\n",
                grid.total_cells, grid.cols, grid.rows, cell_size);

    // ---- Open output ----
    std::string out_dir = gpu_create_output_dir(output_dir);
    std::printf("GPU headless mode: %.0fs (dt=%.4f), output -> %s\n",
                duration, dt, out_dir.c_str());

    std::string csv_path = (fs::path(out_dir) / "metrics.csv").string();
    FILE* csv = std::fopen(csv_path.c_str(), "w");
    if (csv) {
        std::fprintf(csv,
            "frame,time_s,infected,recovered,pct_infected,growth_rate,"
            "sick_centroid_x,sick_centroid_y,sick_alignment,"
            "normal_cohesion,normal_alignment,normal_separation,"
            "doctor_cohesion,doctor_alignment,doctor_separation\n");
    }

    // ---- Simulation loop ----
    float next_csv_write = 0.0f;
    float elapsed = 0.0f;
    int frame = 0;
    int peak_infected = 0;
    float peak_pct = 0.0f;
    int last_infected = 0;
    int last_recovered = 0;

    const float progress_step = std::max(10.0f, duration / 30.0f);
    float next_progress = 0.0f;

    while (elapsed < duration) {
        // Spatial hash build
        gpu_spatial_hash_build(buf, grid);

        // Infected centroid (SeekCentroid mode)
        float centroid_x = 0.0f, centroid_y = 0.0f;
        if (cfg.doctor_behavior == 2) {
            gpu_infected_centroid(buf, centroid_x, centroid_y);
        }

        // Steering
        gpu_steering(buf, grid, cfg, qr_normal, qr_doctor, dt,
                     centroid_x, centroid_y);

        // Integration
        gpu_integrate(buf, cfg, dt);

        // Infection (deferred): copy primary -> scratch, kernel, swap
        CUDA_CHECK(cudaMemcpy(buf.d_infected_sorted, buf.d_infected,
                              count * sizeof(uint8_t), cudaMemcpyDeviceToDevice));
        CUDA_CHECK(cudaMemcpy(buf.d_immunity_sorted, buf.d_immunity,
                              count * sizeof(float), cudaMemcpyDeviceToDevice));
        gpu_infection(buf, grid, cfg, dt);
        std::swap(buf.d_infected, buf.d_infected_sorted);
        std::swap(buf.d_immunity, buf.d_immunity_sorted);

        // Cure (deferred): copy primary -> scratch, kernel, swap
        CUDA_CHECK(cudaMemcpy(buf.d_infected_sorted, buf.d_infected,
                              count * sizeof(uint8_t), cudaMemcpyDeviceToDevice));
        CUDA_CHECK(cudaMemcpy(buf.d_immunity_sorted, buf.d_immunity,
                              count * sizeof(float), cudaMemcpyDeviceToDevice));
        gpu_cure(buf, grid, cfg, dt);
        std::swap(buf.d_infected, buf.d_infected_sorted);
        std::swap(buf.d_immunity, buf.d_immunity_sorted);

        // Spontaneous infection (in-place)
        gpu_spontaneous_infection(buf, cfg, dt);

        // Stats
        gpu_count_stats(buf);
        CUDA_CHECK(cudaDeviceSynchronize());

        int h_inf, h_rec;
        CUDA_CHECK(cudaMemcpy(&h_inf, buf.d_infected_count, sizeof(int), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&h_rec, buf.d_recovered_count, sizeof(int), cudaMemcpyDeviceToHost));

        float pct = static_cast<float>(h_inf) / count;
        float growth = static_cast<float>(h_inf - last_infected) / dt;
        last_infected = h_inf;
        last_recovered = h_rec;

        if (h_inf > peak_infected) peak_infected = h_inf;
        if (pct > peak_pct) peak_pct = pct;

        // CSV
        bool do_csv = (csv_interval <= 0.0f) || (elapsed >= next_csv_write);
        if (do_csv && csv) {
            std::fprintf(csv,
                "%d,%.4f,%d,%d,%.6f,%.6f,0.00,0.00,0.000000,"
                "0.000000,0.000000,0.000000,0.000000,0.000000,0.000000\n",
                frame, elapsed, h_inf, h_rec, pct, growth);
            if (csv_interval > 0.0f) next_csv_write += csv_interval;
        }

        // Progress
        if (elapsed >= next_progress) {
            std::printf("[%.1fs / %.1fs] infected: %d/%d (%.1f%%), recovered: %d\n",
                        elapsed, duration, h_inf, count, pct * 100.0f, h_rec);
            next_progress += progress_step;
        }

        frame++;
        elapsed += dt;
    }

    if (csv) std::fclose(csv);

    // ---- Summary ----
    gpu_snapshot_config(out_dir, config_path);

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
        std::fprintf(sf, "Total recovered:    %d\n\n", last_recovered);
        std::fprintf(sf, "--- Final Swarm Metrics ---\n");
        std::fprintf(sf, "Normal  -- cohesion: 0  alignment: 0  separation: 0\n");
        std::fprintf(sf, "Doctor  -- cohesion: 0  alignment: 0  separation: 0\n");
        std::fclose(sf);
    }

    // ---- Cleanup ----
    gpu_buffers_free(buf);

    auto wall_end = std::chrono::steady_clock::now();
    double wall_secs = std::chrono::duration<double>(wall_end - wall_start).count();
    std::printf("GPU simulation complete. %d frames, %.1f wall-seconds (%.1f sim-fps)\n",
                frame, wall_secs, frame / wall_secs);
}
