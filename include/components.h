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
};

// ============================================================
// SimStats singleton — live counters for the stats overlay
// ============================================================

struct SimStats {
    int normal_alive    = 0;
    int doctor_alive    = 0;
    int dead_total      = 0;
    int dead_normal     = 0;
    int dead_doctor     = 0;
    int newborns_total  = 0;
    int newborns_normal = 0;
    int newborns_doctor = 0;
};
