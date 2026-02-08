#pragma once

#include <cstdint>

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

struct Health {
    float age;
    float lifespan;
};

struct InfectionState {
    float time_infected;   // seconds since infection
    float time_to_death;   // t_death countdown
};

struct ReproductionCooldown {
    float cooldown;
};

// ============================================================
// Tag components — zero-size markers for queries
// ============================================================

struct NormalBoid {};
struct DoctorBoid {};
struct Male {};
struct Female {};
struct Infected {};
struct Alive {};
struct Antivax {};

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

    // --- Initial population ---
    int initial_normal_count       = 200;
    int initial_doctor_count       = 10;

    // --- Boid movement parameters (per-second; converted from master plan per-frame @ 60fps) ---
    float max_speed                = 180.0f;  // 3.0 px/frame * 60fps
    float max_force                = 6.0f;    // 0.1 force/frame * 60fps

    float separation_weight        = 1.5f;
    float alignment_weight         = 1.0f;
    float cohesion_weight          = 1.0f;

    float separation_radius        = 25.0f;
    float alignment_radius         = 50.0f;
    float cohesion_radius          = 50.0f;

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
};

struct SimStats {
    int normal_alive    = 0;
    int doctor_alive    = 0;
    int dead_total      = 0;
    int dead_normal     = 0;
    int dead_doctor     = 0;
    int newborns_total  = 0;
    int newborns_normal = 0;
    int newborns_doctor = 0;

    // Population history for graph (circular buffer)
    static constexpr int HISTORY_SIZE = 500;
    PopulationHistoryPoint history[HISTORY_SIZE] = {};
    int history_index = 0;  // Current write position (wraps around)
    int history_count = 0;  // Number of valid entries (0 to HISTORY_SIZE)
};

// ============================================================
// SimulationState singleton — controls for pause/reset
// ============================================================

struct SimulationState {
    SimulationState() = default;
    bool is_paused = false;
    bool reset_requested = false;  // Set by UI, cleared by main loop after reset
};
