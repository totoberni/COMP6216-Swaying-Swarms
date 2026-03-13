#include "sim/output.h"
#include "components.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// T2a.3 — sim-out/ directory management
// ---------------------------------------------------------------------------

std::string create_output_dir(const char* base) {
    fs::path base_path(base);
    if (!fs::exists(base_path)) {
        fs::create_directories(base_path);
    }

    // Scan for existing outN/ directories, find max N
    int max_n = -1;
    std::regex out_re("^out(\\d+)$");
    for (const auto& entry : fs::directory_iterator(base_path)) {
        if (!entry.is_directory()) continue;
        std::smatch m;
        std::string name = entry.path().filename().string();
        if (std::regex_match(name, m, out_re)) {
            int n = std::stoi(m[1].str());
            if (n > max_n) max_n = n;
        }
    }

    fs::path new_dir = base_path / ("out" + std::to_string(max_n + 1));
    fs::create_directories(new_dir);
    return new_dir.string();
}

// ---------------------------------------------------------------------------
// T2a.4 — CSV time-series export (incremental, called each frame)
// ---------------------------------------------------------------------------

FILE* open_csv(const std::string& dir) {
    std::string path = (fs::path(dir) / "metrics.csv").string();
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "ERROR: Cannot open %s for writing\n", path.c_str());
        return nullptr;
    }
    std::fprintf(f,
        "frame,time_s,infected,recovered,pct_infected,growth_rate,"
        "sick_centroid_x,sick_centroid_y,sick_alignment,"
        "normal_cohesion,normal_alignment,normal_separation,"
        "doctor_cohesion,doctor_alignment,doctor_separation\n");
    return f;
}

void write_csv_row(FILE* f, int frame, float time_s, const SimStats& stats) {
    if (!f) return;
    std::fprintf(f,
        "%d,%.4f,%d,%d,%.6f,%.6f,%.2f,%.2f,%.6f,"
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
        frame, time_s,
        stats.total_infected, stats.total_recovered,
        stats.pct_infected, stats.infection_growth_rate,
        stats.sick_centroid_x, stats.sick_centroid_y,
        stats.sick_avg_alignment,
        stats.swarm[0].average_cohesion,
        stats.swarm[0].average_alignment_angle,
        stats.swarm[0].average_separation,
        stats.swarm[1].average_cohesion,
        stats.swarm[1].average_alignment_angle,
        stats.swarm[1].average_separation);
}

void close_csv(FILE* f) {
    if (f) std::fclose(f);
}

// ---------------------------------------------------------------------------
// T2a.5 — Config snapshot
// ---------------------------------------------------------------------------

void snapshot_config(const std::string& dir, const std::string& config_path) {
    fs::path src(config_path);
    if (!fs::exists(src)) {
        std::fprintf(stderr, "WARNING: config file '%s' not found, skipping snapshot\n",
                     config_path.c_str());
        return;
    }
    fs::path dst = fs::path(dir) / "config_used.ini";
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

// ---------------------------------------------------------------------------
// T2a.6 — Final summary
// ---------------------------------------------------------------------------

void export_summary(const std::string& dir, const SimStats& stats,
                    float duration, int peak_infected, float peak_pct) {
    std::string path = (fs::path(dir) / "summary.txt").string();
    std::ofstream out(path);
    if (!out.is_open()) {
        std::fprintf(stderr, "ERROR: Cannot write summary to %s\n", path.c_str());
        return;
    }

    int total_pop = stats.swarm[0].alive + stats.swarm[1].alive;

    out << "=== Simulation Summary ===\n\n";
    out << "Duration:           " << duration << " s\n";
    out << "Total population:   " << total_pop << "\n\n";
    out << "--- Infection ---\n";
    out << "Peak infected:      " << peak_infected
        << " / " << total_pop
        << " (" << (peak_pct * 100.0f) << "%)\n";
    out << "Final infected:     " << stats.total_infected << "\n";
    out << "Total recovered:    " << stats.total_recovered << "\n\n";
    out << "--- Final Swarm Metrics ---\n";
    out << "Normal  — cohesion: " << stats.swarm[0].average_cohesion
        << "  alignment: " << stats.swarm[0].average_alignment_angle
        << "  separation: " << stats.swarm[0].average_separation << "\n";
    out << "Doctor  — cohesion: " << stats.swarm[1].average_cohesion
        << "  alignment: " << stats.swarm[1].average_alignment_angle
        << "  separation: " << stats.swarm[1].average_separation << "\n";
}
