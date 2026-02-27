#pragma once

#include <cstdint>

#include <raylib.h>
#include <raymath.h>

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
    float time_to_death;   // t_death countdown
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

    // --- Reproduction probabilities ---
    float p_offspring_normal       = 0.4f;
    float p_offspring_doctor       = 0.05f;

    // --- Cure & transition probabilities ---
    float p_cure                   = 0.8f;
    float p_become_doctor          = 0.05f;
    float p_antivax                = 0.1f;

    // --- Interaction radii (pixels) ---
    float r_interact_normal        = 30.0f;
    float r_interact_doctor        = 40.0f;

    // --- Time parameters (seconds; converted from master plan frame counts @ 60fps) ---
    float t_death                  = 5.0f;   // 300 frames / 60fps = 5s
    float t_adult                  = 8.33f;  // 500 frames / 60fps ≈ 8.33s

    // --- Offspring count distributions (Normal distribution params) ---
    float offspring_mean_normal    = 2.0f;
    float offspring_stddev_normal  = 1.0f;
    float offspring_mean_doctor    = 1.0f;
    float offspring_stddev_doctor  = 1.0f;

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

    // --- Reproduction cooldown ---
    float reproduction_cooldown    = 5.0f;  // seconds between reproductions

    // --- Infected debuff multipliers ---
    float debuff_p_cure_infected       = 0.5f;  // Doctor p_cure multiplier when infected
    float debuff_r_interact_doctor_infected = 0.7f;  // Doctor interaction radius multiplier when infected
    float debuff_p_offspring_doctor_infected = 0.5f;  // Doctor reproduction probability multiplier when infected
    float debuff_r_interact_normal_infected = 0.8f;  // Normal interaction radius multiplier when infected
    float debuff_p_offspring_normal_infected = 0.5f;  // Normal reproduction probability multiplier when infected

    // --- Antivax parameters ---
    float antivax_repulsion_radius     = 100.0f;  // Visual range for detecting doctors
    float antivax_repulsion_weight     = 3.0f;    // Strength of repulsion force (additive to flocking)
};

// ============================================================
// SimStats singleton — live counters for the stats overlay
// ============================================================

struct PopulationHistoryPoint {
    int normal_alive = 0;
    int doctor_alive = 0;
    int antivax_alive = 0;
    int infected_count = 0;
};

struct SimStats {
    int normal_alive    = 0;
    int doctor_alive    = 0;
    // int dead_total      = 0;
    // int dead_normal     = 0;
    // int dead_doctor     = 0;
    // int newborns_total  = 0;
    // int newborns_normal = 0;
    // int newborns_doctor = 0;
    // int antivax_alive   = 0;
    // int dead_antivax    = 0;
    // int newborns_antivax = 0;

    // Population history for graph (circular buffer)
    static constexpr int HISTORY_SIZE = 500;
    PopulationHistoryPoint history[HISTORY_SIZE] = {};
    int history_index = 0;  // Current write position (wraps around)
    int history_count = 0;  // Number of valid entries (0 to HISTORY_SIZE)

    // Average Cohesion
    Vector2 pos_avg = Vector2Zero();
    float average_cohesion = 0.;
    // Cohesion history for graph (circular buffer)
    // reuse HISTORY_SIZE = 500;
    float coh_history[HISTORY_SIZE] = {};
    int coh_history_index = 0;  // Current write position (wraps around)
    int coh_history_count = 0;  // Number of valid entries (0 to HISTORY_SIZE)

    // Average Alignment
    Vector2 vel_avg = Vector2Zero();
    float average_alignment_angle = 0.;
    // Alignment history for graph (circular buffer)
    // reuse HISTORY_SIZE = 500;
    float ali_history[HISTORY_SIZE] = {};
    int ali_history_index = 0;  // Current write position (wraps around)
    int ali_history_count = 0;  // Number of valid entries (0 to HISTORY_SIZE)

    // Average Separation (RMS pairwise distance via Huygens-Steiner)
    float average_separation = 0.0f;
    float sep_history[HISTORY_SIZE] = {};
    int sep_history_index = 0;
    int sep_history_count = 0;
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
