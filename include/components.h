#pragma once

#include <cstdint>
#include <cstring>

#include <raylib.h>
#include <raymath.h>

// ============================================================
// Behavior enums — used by SimConfig, defined before it
// ============================================================

enum class DoctorBehavior { Normal, SeekNearest, SeekCentroid };
enum class SwarmBehavior { Simple, Oval, Chaotic };

// ============================================================
// Core components — attached to every boid entity
// ============================================================

struct Position {
    float x, y;
};

struct Velocity {
    float vx, vy;
};

struct Heading {
    float angle; // radians
};

struct InfectionState {
    float time_infected;   // seconds since infection
};

struct ImmunityState {
    float immunity_level;       // 1.0 = fully immune, decays to 0.0
    float time_since_recovery;  // seconds since recovery
};

// ============================================================
// Tag components — zero-size markers for queries
// ============================================================

struct NormalBoid {};
struct DoctorBoid {};
struct Infected {};

// ============================================================
// SimConfig singleton — ALL tunable simulation parameters
// ============================================================

struct SimConfig {
    // --- Initial infection probabilities ---
    float p_initial_infect_normal  = 0.05f;
    float p_initial_infect_doctor  = 0.02f;

    // --- Interaction infection probabilities ---
    float p_infect_normal          = 0.5f;
    float p_infect_doctor          = 0.5f;

    // --- Cure probability ---
    float p_cure                   = 0.8f;

    // --- Interaction radii (pixels) ---
    float r_interact_normal        = 30.0f;
    float r_interact_doctor        = 40.0f;

    // --- World bounds ---
    float world_width              = 1920.0f;
    float world_height             = 1080.0f;
    bool wall_bounce               = true; // If true, boids bounce off walls instead of wrapping around

    // --- Initial population ---
    int initial_normal_count       = 200;
    int initial_doctor_count       = 10;

    // --- Boid movement (Shiffman/Processing.org Model B, scaled to per-second @ 60fps) ---
    float max_speed                = 180.0f;  // Shiffman maxspeed=3 * 60fps
    float max_force                = 180.0f;  // Shiffman maxforce=0.05 * 60^2 (preserves per-frame steering ratio)
    float min_speed                = 54.0f;   // 30% of max_speed — prevents stalling

    float separation_weight        = 1.5f;    // Shiffman default
    float alignment_weight         = 1.0f;    // Shiffman default
    float cohesion_weight          = 1.0f;    // Shiffman default

    float separation_radius        = 25.0f;   // Shiffman desiredSeparation
    float alignment_radius         = 50.0f;   // Shiffman neighbor distance
    float cohesion_radius          = 50.0f;   // Shiffman neighbor distance

    float fov                      = 1.05f; // 1/2 of fov angle in radians

    // --- Infected debuff multipliers ---
    float debuff_p_cure_infected       = 0.5f;  // Doctor p_cure multiplier when infected
    float debuff_r_interact_doctor_infected = 0.7f;  // Doctor interaction radius multiplier when infected
    float debuff_r_interact_normal_infected = 0.8f;  // Normal interaction radius multiplier when infected

    // --- Cure immunity (SIR: permanent immunity after cure) ---
    float cure_immunity_level          = 1.0f;    // Immunity granted by doctor cure (0.0-1.0)

    // --- Doctor behavior (stubs for P2) ---
    DoctorBehavior doctor_behavior     = DoctorBehavior::Normal;
    float doctor_seek_radius           = 300.0f;  // D2/D3: how far doctors scan for sick
    float doctor_seek_weight           = 5.0f;    // D2/D3: force multiplier for seeking

    // --- Swarm behavior (stubs for P2) ---
    SwarmBehavior swarm_behavior       = SwarmBehavior::Simple;
    float noise_factor                 = 0.0f;    // B3 chaotic: random force magnitude

    // --- Headless mode (stubs for P2) ---
    bool nogui                         = false;
    float nogui_duration               = 300.0f;  // seconds (5 min default)

    // --- Output ---
    char output_dir[256]               = "sim-out";
};

// ============================================================
// SimStats singleton — live counters for the stats overlay
// ============================================================

struct PopulationHistoryPoint {
    int normal_alive = 0;
    int doctor_alive = 0;
    int infected_count = 0;
};

struct SwarmMetrics {
    int alive = 0;
    Vector2 pos_avg = Vector2Zero();
    Vector2 vel_avg = Vector2Zero();
    float average_cohesion = 0.0f;
    float average_alignment_angle = 0.0f;
    float average_separation = 0.0f;

    static constexpr int HISTORY_SIZE = 500;
    float coh_history[HISTORY_SIZE] = {};
    int coh_history_index = 0, coh_history_count = 0;
    float ali_history[HISTORY_SIZE] = {};
    int ali_history_index = 0, ali_history_count = 0;
    float sep_history[HISTORY_SIZE] = {};
    int sep_history_index = 0, sep_history_count = 0;
};

struct SimStats {
    SwarmMetrics swarm[2];  // [0]=normal, [1]=doctor

    // Population history for graph (circular buffer)
    static constexpr int HISTORY_SIZE = 500;
    PopulationHistoryPoint history[HISTORY_SIZE] = {};
    int history_index = 0;  // Current write position (wraps around)
    int history_count = 0;  // Number of valid entries (0 to HISTORY_SIZE)

    // Sickness metrics (updated per frame by UpdateStatsSystem)
    int total_infected = 0;
    int total_recovered = 0;          // SIR: permanently immune count
    float pct_infected = 0.0f;        // infected / total population
    float infection_growth_rate = 0.0f; // delta(infected)/delta(t), smoothed
    float sick_centroid_x = 0.0f;
    float sick_centroid_y = 0.0f;
    float sick_avg_alignment = 0.0f;  // average heading angle of infected boids

    // Sickness history buffers (circular, same HISTORY_SIZE)
    float infected_count_history[HISTORY_SIZE]{};
    float pct_infected_history[HISTORY_SIZE]{};
    float growth_rate_history[HISTORY_SIZE]{};
    float recovered_count_history[HISTORY_SIZE]{};
};

// ============================================================
// SimulationState singleton — controls for pause/reset
// ============================================================

struct SimulationState {
    SimulationState() = default;
    bool is_paused = false;
    bool reset_requested = false;  // Set by UI, cleared by main loop after reset
    bool show_stats_overlay = false; // Toggle for stats overlay
    bool show_radii = true; // Toggle for interaction radius circles (V key)
};
