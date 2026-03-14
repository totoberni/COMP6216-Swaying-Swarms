#include "sim/sweep.h"
#include "sim/rng.h"
#include "sim/output.h"
#include "ecs/world.h"
#include "ecs/systems.h"
#include "ecs/spawn.h"
#include "ecs/stats.h"
#include "components.h"
#include "config_loader.h"

#include <flecs.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// Formation preset cache — read once from canonical B*_D1.ini
// ============================================================

struct FormationPreset {
    float fov;
    float alignment_weight;
    float cohesion_weight;
    float noise_factor;
};

static std::map<std::string, FormationPreset> load_formation_presets(
    const std::string& base_config_dir)
{
    std::map<std::string, FormationPreset> presets;
    const std::vector<std::pair<std::string, std::string>> formations = {
        {"B1", "B1_D1.ini"},
        {"B2", "B2_D1.ini"},
        {"B3", "B3_D1.ini"},
    };

    for (const auto& [name, filename] : formations) {
        std::string path = (fs::path(base_config_dir) / filename).string();
        SimConfig cfg{};
        if (!load_config(path, cfg)) {
            std::fprintf(stderr, "WARNING: Cannot load formation preset from %s\n",
                         path.c_str());
            continue;
        }
        presets[name] = FormationPreset{
            cfg.normal.fov,
            cfg.normal.alignment_weight,
            cfg.normal.cohesion_weight,
            cfg.normal.noise_factor,
        };
    }

    if (presets.size() != 3) {
        std::fprintf(stderr, "ERROR: Expected 3 formation presets, got %zu\n",
                     presets.size());
    }
    return presets;
}

// ============================================================
// Sweep parameter definitions
// ============================================================

enum class ParamType { Uniform, LogUniform, Int };

struct SweepParam {
    std::string name;
    float min_val;
    float max_val;
    ParamType type;
};

static const std::vector<SweepParam> SWEEP_PARAMS = {
    {"p_cure",               0.1f,   1.0f,   ParamType::Uniform},
    {"doctor_seek_radius",   0.0f,   500.0f, ParamType::Uniform},
    {"doctor_seek_weight",   0.5f,   10.0f,  ParamType::LogUniform},
    {"initial_doctor_count", 3.0f,   40.0f,  ParamType::Int},
    {"r_interact_doctor",    20.0f,  120.0f, ParamType::Uniform},
};

static const std::vector<std::string> DOCTOR_BEHAVIORS = {"normal", "nearest", "centroid"};
static const std::vector<std::string> FORMATIONS = {"B1", "B2", "B3"};

// ============================================================
// LHS sampling
// ============================================================

// Generate n×d LHS samples in [0,1]^d
static std::vector<std::vector<float>> generate_lhs_unit(
    int n, int d, std::mt19937& rng)
{
    std::vector<std::vector<float>> samples(n, std::vector<float>(d));
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    for (int dim = 0; dim < d; ++dim) {
        // Random permutation of [0, n-1]
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);

        for (int i = 0; i < n; ++i) {
            samples[perm[i]][dim] = (static_cast<float>(i) + u01(rng))
                                    / static_cast<float>(n);
        }
    }
    return samples;
}

struct SampleSet {
    std::vector<std::map<std::string, float>> continuous;
    std::vector<std::string> doctor_behaviors;
    std::vector<std::string> formations;
};

static SampleSet generate_samples(int n, uint32_t seed) {
    std::mt19937 rng(seed);

    int n_continuous = static_cast<int>(SWEEP_PARAMS.size());
    auto unit_samples = generate_lhs_unit(n, n_continuous, rng);

    SampleSet result;
    result.continuous.resize(n);

    // Map [0,1] to actual param values
    for (int i = 0; i < n; ++i) {
        for (int d = 0; d < n_continuous; ++d) {
            float u = unit_samples[i][d];
            const auto& sp = SWEEP_PARAMS[d];
            float val;

            switch (sp.type) {
                case ParamType::Uniform:
                    val = sp.min_val + u * (sp.max_val - sp.min_val);
                    break;
                case ParamType::LogUniform: {
                    float log_min = std::log(sp.min_val);
                    float log_max = std::log(sp.max_val);
                    val = std::exp(log_min + u * (log_max - log_min));
                    break;
                }
                case ParamType::Int:
                    val = std::round(sp.min_val + u * (sp.max_val - sp.min_val));
                    break;
            }
            result.continuous[i][sp.name] = val;
        }
    }

    // Categorical: balanced round-robin with shuffle
    result.doctor_behaviors.resize(n);
    result.formations.resize(n);

    // Doctor behaviors — balanced assignment
    {
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);
        for (int i = 0; i < n; ++i) {
            result.doctor_behaviors[indices[i]] =
                DOCTOR_BEHAVIORS[i % DOCTOR_BEHAVIORS.size()];
        }
    }

    // Formations — balanced assignment
    {
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);
        for (int i = 0; i < n; ++i) {
            result.formations[indices[i]] =
                FORMATIONS[i % FORMATIONS.size()];
        }
    }

    return result;
}

// ============================================================
// INI config reader/writer (raw text, preserves structure)
// ============================================================

// Simple INI representation for read-modify-write
struct IniFile {
    struct Section {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };
    std::vector<std::string> preamble; // lines before first section
    std::vector<Section> sections;

    bool has_section(const std::string& name) const {
        for (const auto& s : sections)
            if (s.name == name) return true;
        return false;
    }

    Section& get_or_create(const std::string& name) {
        for (auto& s : sections)
            if (s.name == name) return s;
        sections.push_back({name, {}});
        return sections.back();
    }

    void set(const std::string& section, const std::string& key,
             const std::string& value) {
        auto& sec = get_or_create(section);
        for (auto& [k, v] : sec.entries) {
            if (k == key) { v = value; return; }
        }
        sec.entries.emplace_back(key, value);
    }

    void write(const std::string& path) const {
        std::ofstream out(path);
        for (const auto& line : preamble) out << line << "\n";
        for (const auto& sec : sections) {
            out << "\n[" << sec.name << "]\n";
            for (const auto& [k, v] : sec.entries) {
                out << k << " = " << v << "\n";
            }
        }
    }
};

static IniFile parse_ini(const std::string& path) {
    IniFile ini;
    std::ifstream in(path);
    if (!in.is_open()) {
        std::fprintf(stderr, "ERROR: Cannot open %s\n", path.c_str());
        return ini;
    }

    std::string line;
    IniFile::Section* current = nullptr;

    while (std::getline(in, line)) {
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '
                                  || line.back() == '\t'))
            line.pop_back();

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            if (!current) ini.preamble.push_back(line);
            continue;
        }

        // Section header
        if (line[0] == '[') {
            auto end = line.find(']');
            if (end != std::string::npos) {
                std::string name = line.substr(1, end - 1);
                ini.sections.push_back({name, {}});
                current = &ini.sections.back();
            }
            continue;
        }

        // Key = value
        if (current) {
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                // Trim
                while (!key.empty() && key.back() == ' ') key.pop_back();
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                // Strip inline comments
                auto comment = val.find_first_of(";#");
                if (comment != std::string::npos) val = val.substr(0, comment);
                while (!val.empty() && val.back() == ' ') val.pop_back();
                current->entries.emplace_back(key, val);
            }
        }
    }
    return ini;
}

// ============================================================
// Config writer
// ============================================================

static std::string float_str(float v, int precision = 4) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, v);
    return buf;
}

static void write_sweep_config(
    const IniFile& base_ini,
    const std::map<std::string, float>& params,
    const std::string& doctor_behavior,
    const std::string& formation,
    const FormationPreset& preset,
    float duration,
    const std::string& output_path,
    const std::string& run_output_dir)
{
    IniFile ini = base_ini; // copy

    // Continuous param overrides
    auto it = params.find("p_cure");
    if (it != params.end())
        ini.set("cure", "p_cure", float_str(it->second));

    it = params.find("r_interact_doctor");
    if (it != params.end())
        ini.set("interaction", "r_interact_doctor", float_str(it->second, 1));

    it = params.find("initial_doctor_count");
    if (it != params.end())
        ini.set("population", "initial_doctor_count",
                std::to_string(static_cast<int>(it->second)));

    it = params.find("doctor_seek_radius");
    if (it != params.end())
        ini.set("doctor_swarm", "doctor_seek_radius", float_str(it->second, 1));

    it = params.find("doctor_seek_weight");
    if (it != params.end())
        ini.set("doctor_swarm", "doctor_seek_weight", float_str(it->second, 2));

    // Doctor behavior
    ini.set("doctor_swarm", "behavior", doctor_behavior);

    // Formation steering params (from canonical configs)
    ini.set("normal_swarm", "fov", float_str(preset.fov, 2));
    ini.set("normal_swarm", "alignment_weight", float_str(preset.alignment_weight, 1));
    ini.set("normal_swarm", "cohesion_weight", float_str(preset.cohesion_weight, 1));
    ini.set("normal_swarm", "noise_factor", float_str(preset.noise_factor, 1));

    // Duration + headless
    ini.set("headless", "nogui_duration", float_str(duration, 1));
    ini.set("headless", "nogui", "true");
    ini.set("headless", "csv_sample_interval", "0.5");

    // Output dir for this run
    ini.set("output", "output_dir", run_output_dir);

    fs::create_directories(fs::path(output_path).parent_path());
    ini.write(output_path);
}

// ============================================================
// Manifest writer (JSON, backward-compatible with Python)
// ============================================================

static void write_manifest(
    const std::string& path,
    const SampleSet& samples,
    int n_samples,
    const std::string& base_config,
    float duration,
    uint32_t seed)
{
    std::ofstream out(path);
    out << "{\n";
    out << "  \"n_samples\": " << n_samples << ",\n";
    out << "  \"base_config\": \"" << base_config << "\",\n";
    out << "  \"duration\": " << float_str(duration, 1) << ",\n";
    out << "  \"seed\": " << seed << ",\n";
    out << "  \"params\": {\n";

    for (int i = 0; i < n_samples; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "sweep_%03d.ini", i);

        out << "    \"" << name << "\": {\n";
        // Continuous params
        for (const auto& sp : SWEEP_PARAMS) {
            float val = samples.continuous[i].at(sp.name);
            if (sp.type == ParamType::Int) {
                out << "      \"" << sp.name << "\": "
                    << static_cast<int>(val);
            } else {
                out << "      \"" << sp.name << "\": " << val;
            }
            out << ",\n";
        }
        // Categorical params
        out << "      \"doctor_behavior\": \""
            << samples.doctor_behaviors[i] << "\",\n";
        out << "      \"boid_formation\": \""
            << samples.formations[i] << "\"\n";
        out << "    }";
        if (i < n_samples - 1) out << ",";
        out << "\n";
    }

    out << "  }\n";
    out << "}\n";
}

// ============================================================
// Headless runner (reuse existing run_headless from main.cpp)
// We need a self-contained version since the original is static.
// ============================================================

static void run_single_headless(flecs::world& world, const SimConfig& config,
                                const std::string& config_path) {
    const float dt = (config.headless_dt > 0.0f) ? config.headless_dt : (1.0f / 60.0f);
    const float duration = config.nogui_duration;

    std::string out_dir = create_output_dir(config.output_dir);

    FILE* csv = open_csv(out_dir);
    const float csv_interval = config.csv_sample_interval;
    float next_csv_write = 0.0f;

    float elapsed = 0.0f;
    int frame = 0;

    int peak_infected = 0;
    float peak_pct = 0.0f;

    while (elapsed < duration) {
        world.progress(dt);

        const SimStats& stats = world.get<SimStats>();

        bool do_write = (csv_interval <= 0.0f) || (elapsed >= next_csv_write);
        if (do_write) {
            write_csv_row(csv, frame, elapsed, stats);
            if (csv_interval > 0.0f) next_csv_write += csv_interval;
        }

        if (stats.total_infected > peak_infected)
            peak_infected = stats.total_infected;
        if (stats.pct_infected > peak_pct)
            peak_pct = stats.pct_infected;

        frame++;
        elapsed += dt;
    }

    close_csv(csv);

    const SimStats& final_stats = world.get<SimStats>();
    snapshot_config(out_dir, config_path);
    export_summary(out_dir, final_stats, duration, peak_infected, peak_pct);
}

// ============================================================
// Post-sweep summary CSV
// ============================================================

static void write_sweep_summary(
    const std::string& output_dir,
    const SampleSet& samples,
    int n_samples)
{
    std::string path = (fs::path(output_dir) / "sweep_summary.csv").string();
    std::ofstream out(path);
    out << "run_dir,formation,doctor_type,peak_infected,peak_pct,"
           "steady_state_infected,steady_state_pct,convergence_time,duration\n";

    for (int i = 0; i < n_samples; ++i) {
        std::string run_dir = (fs::path(output_dir) / ("out" + std::to_string(i))).string();
        std::string summary_path = (fs::path(run_dir) / "summary.txt").string();

        // Parse summary.txt for metrics
        int peak_infected = 0;
        float peak_pct = 0.0f;
        int final_infected = 0;
        int total_pop = 0;
        float run_duration = 0.0f;

        std::ifstream summary(summary_path);
        if (summary.is_open()) {
            std::string line;
            while (std::getline(summary, line)) {
                if (line.find("Peak infected:") != std::string::npos) {
                    // Format: "Peak infected:      N / M (P%)"
                    auto colon = line.find(':');
                    if (colon != std::string::npos) {
                        std::string rest = line.substr(colon + 1);
                        if (std::sscanf(rest.c_str(), " %d / %d (%f",
                                        &peak_infected, &total_pop, &peak_pct) >= 2) {
                            peak_pct /= 100.0f;
                        }
                    }
                } else if (line.find("Final infected:") != std::string::npos) {
                    auto colon = line.find(':');
                    if (colon != std::string::npos)
                        std::sscanf(line.substr(colon + 1).c_str(), " %d", &final_infected);
                } else if (line.find("Duration:") != std::string::npos) {
                    auto colon = line.find(':');
                    if (colon != std::string::npos)
                        std::sscanf(line.substr(colon + 1).c_str(), " %f", &run_duration);
                }
            }
        }

        float steady_pct = (total_pop > 0)
            ? static_cast<float>(final_infected) / static_cast<float>(total_pop)
            : 0.0f;

        out << "out" << i << ","
            << samples.formations[i] << ","
            << samples.doctor_behaviors[i] << ","
            << peak_infected << ","
            << peak_pct << ","
            << final_infected << ","
            << steady_pct << ","
            << 0.0f << ","  // convergence_time — requires metrics.csv analysis
            << run_duration << "\n";
    }
}

// ============================================================
// Thread-pool sweep runner
// ============================================================

void run_sweep(const SweepConfig& config) {
    auto t_start = std::chrono::steady_clock::now();

    std::printf("=== C++ Monte Carlo Sweep ===\n");
    std::printf("  Base config: %s\n", config.base_config_path.c_str());
    std::printf("  Samples: %d\n", config.n_samples);
    std::printf("  Threads: %d\n", config.threads);
    std::printf("  Seed: %u\n", config.seed);
    std::printf("  Duration: %.1f s\n", config.duration);
    std::printf("  Output: %s\n\n", config.output_dir.c_str());

    // Resolve config directory for formation presets
    fs::path base_path(config.base_config_path);
    std::string config_dir = base_path.parent_path().string();
    if (config_dir.empty()) config_dir = ".";

    // Load formation presets from canonical B*_D1.ini files
    auto presets = load_formation_presets(config_dir);
    if (presets.size() != 3) {
        std::fprintf(stderr, "FATAL: Could not load all formation presets\n");
        return;
    }

    // Parse base config as INI template
    IniFile base_ini = parse_ini(config.base_config_path);

    // Generate LHS samples
    std::printf("Generating %d LHS samples...\n", config.n_samples);
    SampleSet samples = generate_samples(config.n_samples, config.seed);

    // Create output directories and write configs
    fs::create_directories(config.output_dir);
    std::string configs_dir = (fs::path(config.output_dir) / "configs").string();
    fs::create_directories(configs_dir);

    for (int i = 0; i < config.n_samples; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "sweep_%03d.ini", i);
        std::string cfg_path = (fs::path(configs_dir) / name).string();
        std::string run_out = (fs::path(config.output_dir)).string();

        write_sweep_config(
            base_ini,
            samples.continuous[i],
            samples.doctor_behaviors[i],
            samples.formations[i],
            presets.at(samples.formations[i]),
            config.duration,
            cfg_path,
            run_out);
    }

    // Write manifest
    std::string manifest_path = (fs::path(configs_dir) / "manifest.json").string();
    write_manifest(manifest_path, samples, config.n_samples,
                   config.base_config_path, config.duration, config.seed);
    std::printf("Generated %d configs + manifest in %s/\n\n",
                config.n_samples, configs_dir.c_str());

    // Thread-pool execution
    std::atomic<int> next_run{0};
    std::atomic<int> completed{0};
    std::mutex print_mutex;

    auto worker = [&](int thread_id) {
        while (true) {
            int run_idx = next_run.fetch_add(1);
            if (run_idx >= config.n_samples) break;

            auto run_start = std::chrono::steady_clock::now();

            // Seed this thread's RNG
            seed_sim_rng(config.seed + static_cast<uint32_t>(run_idx));

            // Load the sweep config for this run
            char cfg_name[32];
            std::snprintf(cfg_name, sizeof(cfg_name), "sweep_%03d.ini", run_idx);
            std::string cfg_path = (fs::path(configs_dir) / cfg_name).string();

            // Create fresh FLECS world
            flecs::world world;
            init_world(world, cfg_path);
            register_all_systems(world);
            register_stats_system(world);
            spawn_initial_population(world);

            // Run headless simulation
            const SimConfig& sim_cfg = world.get<SimConfig>();
            run_single_headless(world, sim_cfg, cfg_path);

            auto run_end = std::chrono::steady_clock::now();
            float run_secs = std::chrono::duration<float>(run_end - run_start).count();

            int done = completed.fetch_add(1) + 1;
            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::printf("[%d/%d] Run %03d complete (%.1fs)\n",
                            done, config.n_samples, run_idx, run_secs);
            }
        }
    };

    // Launch thread pool
    std::printf("Starting %d worker threads...\n", config.threads);
    std::vector<std::thread> threads;
    threads.reserve(config.threads);
    for (int t = 0; t < config.threads; ++t) {
        threads.emplace_back(worker, t);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto t_end = std::chrono::steady_clock::now();
    float total_secs = std::chrono::duration<float>(t_end - t_start).count();

    std::printf("\n=== Sweep Complete ===\n");
    std::printf("  Total time: %.1f s\n", total_secs);
    std::printf("  Runs/sec: %.2f\n",
                static_cast<float>(config.n_samples) / total_secs);

    // Post-sweep summary
    std::printf("Writing sweep summary...\n");
    write_sweep_summary(config.output_dir, samples, config.n_samples);
    std::printf("Done. Results in %s/\n", config.output_dir.c_str());
}
