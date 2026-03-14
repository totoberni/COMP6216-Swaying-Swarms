#include "ecs/world.h"
#include "ecs/systems.h"
#include "ecs/spawn.h"
#include "ecs/stats.h"
#include "render/renderer.h"
#include "sim/output.h"
#include "sim/sweep.h"
#include "components.h"
#include "render_state.h"
#include <flecs.h>
#include <raylib.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------

struct CliArgs {
    std::string config_path = "config.ini";
    bool nogui = false;
    // Sweep mode
    bool sweep = false;
    int n_samples = 600;
    int threads = 0;  // 0 = auto-detect
    uint32_t seed = 42;
    float duration = 0.0f;  // 0 = use config's nogui_duration
    std::string output_dir = "sim-out";
};

static void print_usage(const char* prog) {
    std::printf("Usage: %s [options] [config.ini]\n\n", prog);
    std::printf("Single-run mode:\n");
    std::printf("  %s config.ini          Run with GUI\n", prog);
    std::printf("  %s -nogui config.ini   Run headless\n\n", prog);
    std::printf("Sweep mode:\n");
    std::printf("  --sweep                Enable Monte Carlo sweep mode\n");
    std::printf("  --n-samples N          Number of LHS samples (default: 600)\n");
    std::printf("  --threads T            Worker threads (default: cores - 2)\n");
    std::printf("  --seed S               RNG seed (default: 42)\n");
    std::printf("  --duration D           Sim duration in seconds (default: config value)\n");
    std::printf("  --output-dir DIR       Output directory (default: sim-out)\n");
    std::printf("  --help                 Show this help\n");
}

static CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-nogui") == 0) {
            args.nogui = true;
        } else if (std::strcmp(argv[i], "--sweep") == 0) {
            args.sweep = true;
        } else if (std::strcmp(argv[i], "--n-samples") == 0 && i + 1 < argc) {
            args.n_samples = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            args.threads = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            args.seed = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            args.duration = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            std::exit(0);
        } else if (argv[i][0] != '-') {
            args.config_path = argv[i];
        }
    }
    return args;
}

// ---------------------------------------------------------------------------
// Headless simulation loop (T2a.2)
// ---------------------------------------------------------------------------

static void run_headless(flecs::world& world, const SimConfig& config,
                         const std::string& config_path) {
    const float dt = (config.headless_dt > 0.0f) ? config.headless_dt : (1.0f / 60.0f);
    const float duration = config.nogui_duration;

    // Create output directory
    std::string out_dir = create_output_dir(config.output_dir);
    std::printf("Headless mode: %.0fs (dt=%.4f), output -> %s\n", duration, dt, out_dir.c_str());

    // Open CSV for incremental writes
    FILE* csv = open_csv(out_dir);

    // CSV sampling: interval <= 0 means write every frame (backward compat)
    const float csv_interval = config.csv_sample_interval;
    float next_csv_write = 0.0f;

    float elapsed = 0.0f;
    int frame = 0;
    const float progress_step = std::max(10.0f, duration / 30.0f);
    float next_progress = 0.0f;

    // Peak tracking (not stored in SimStats — tracked here)
    int peak_infected = 0;
    float peak_pct = 0.0f;

    while (elapsed < duration) {
        world.progress(dt);

        const SimStats& stats = world.get<SimStats>();

        // Write CSV row at configured interval (or every frame if interval <= 0)
        bool write_csv = (csv_interval <= 0.0f) || (elapsed >= next_csv_write);
        if (write_csv) {
            write_csv_row(csv, frame, elapsed, stats);
            if (csv_interval > 0.0f) next_csv_write += csv_interval;
        }

        // Track peaks
        if (stats.total_infected > peak_infected) {
            peak_infected = stats.total_infected;
        }
        if (stats.pct_infected > peak_pct) {
            peak_pct = stats.pct_infected;
        }

        // Progress output
        if (elapsed >= next_progress) {
            int total_pop = stats.swarm[0].alive + stats.swarm[1].alive;
            std::printf("[%.1fs / %.1fs] infected: %d/%d (%.1f%%), recovered: %d\n",
                        elapsed, duration,
                        stats.total_infected, total_pop,
                        stats.pct_infected * 100.0f,
                        stats.total_recovered);
            next_progress += progress_step;
        }

        frame++;
        elapsed += dt;
    }

    close_csv(csv);

    // Finalize output
    const SimStats& final_stats = world.get<SimStats>();
    snapshot_config(out_dir, config_path);
    export_summary(out_dir, final_stats, duration, peak_infected, peak_pct);

    std::printf("Simulation complete. %d frames written to %s\n",
                frame, out_dir.c_str());
}

// ---------------------------------------------------------------------------
// GUI simulation loop (existing logic, extracted)
// ---------------------------------------------------------------------------

static void run_gui(flecs::world& world, const SimConfig& config) {
    init_renderer(static_cast<int>(config.world_width),
                  static_cast<int>(config.world_height),
                  "COMP6216 Boid Swarm");

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        SimulationState& sim_state = world.get_mut<SimulationState>();

        // Keyboard shortcuts
        if (IsKeyPressed(KEY_SPACE)) {
            sim_state.is_paused = !sim_state.is_paused;
        }
        if (IsKeyPressed(KEY_R)) {
            sim_state.reset_requested = true;
        }
        if (IsKeyPressed(KEY_H)) {
            sim_state.show_stats_overlay = !sim_state.show_stats_overlay;
        }
        if (IsKeyPressed(KEY_V)) {
            sim_state.show_radii = !sim_state.show_radii;
        }

        if (sim_state.reset_requested) {
            reset_simulation(world);
            sim_state.reset_requested = false;
            sim_state.is_paused = false;
        }

        if (!sim_state.is_paused) {
            world.progress(dt);
        }

        const RenderState& rs = world.get<RenderState>();
        render_frame(rs);
    }

    close_renderer();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    CliArgs args = parse_args(argc, argv);

    // Sweep mode — parallel Monte Carlo runs
    if (args.sweep) {
        SweepConfig sc;
        sc.base_config_path = args.config_path;
        sc.output_dir = args.output_dir;
        sc.n_samples = args.n_samples;
        sc.threads = args.threads > 0
            ? args.threads
            : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);
        sc.seed = args.seed;
        if (args.duration > 0.0f) sc.duration = args.duration;
        run_sweep(sc);
        return 0;
    }

    // Single-run mode (existing logic)
    flecs::world world;
    init_world(world, args.config_path);
    register_all_systems(world);
    register_stats_system(world);

    spawn_initial_population(world);

    SimConfig& config = world.get_mut<SimConfig>();
    if (args.nogui) {
        config.nogui = true;
    }

    if (config.nogui) {
        run_headless(world, config, args.config_path);
    } else {
        run_gui(world, config);
    }

    return 0;
}
