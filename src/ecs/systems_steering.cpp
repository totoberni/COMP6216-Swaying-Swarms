#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "toroidal.h"
#include "../sim/rng.h"
#include <flecs.h>
#include <cmath>
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <random>

// Helper: compute Reynolds steering force (normalize→scale→subtract→clamp pattern)
static Vector2 steer_toward(Vector2 desired_dir, float max_speed, Vector2 current_vel, float max_force) {
    float mag = Vector2Length(desired_dir);
    if (mag < 0.001f) return Vector2Zero();
    Vector2 desired = Vector2Scale(desired_dir, max_speed / mag);
    Vector2 steer = Vector2Subtract(desired, current_vel);
    float steer_mag = Vector2Length(steer);
    if (steer_mag > max_force) {
        steer = Vector2Scale(steer, max_force / steer_mag);
    }
    return steer;
}

// ============================================================
// PreUpdate Phase: Spatial Grid Rebuild (enriched entries)
// ============================================================

void register_rebuild_grid_system(flecs::world& world) {
    world.system("RebuildGridSystem")
        .kind(flecs::PreUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SpatialGrid& grid = w.get_mut<SpatialGrid>();

            grid.clear();

            auto q = w.query<const Position, const Velocity>();
            q.each([&grid](flecs::entity e, const Position& pos, const Velocity& vel) {
                uint8_t swarm_type = e.has<DoctorBoid>() ? 1 : 0;
                bool infected = e.has<Infected>();

                grid.insert(e.id(), pos.x, pos.y, vel.vx, vel.vy, swarm_type, infected);
            });
        });
}

// ============================================================
// OnUpdate Phase: Steering and Movement
// ============================================================

void register_steering_system(flecs::world& world) {
    world.system("SteeringSystem")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            float dt = it.delta_time();

            bool toroidal = !config.wall_bounce;
            float ww = config.world_width, wh = config.world_height;

            // Use the larger radii from both swarms for grid queries
            float query_radius = std::max({config.normal.separation_radius,
                                            config.normal.alignment_radius,
                                            config.normal.cohesion_radius,
                                            config.doctor.separation_radius,
                                            config.doctor.alignment_radius,
                                            config.doctor.cohesion_radius});

            std::vector<SpatialGrid::QueryResult> neighbors;
            neighbors.reserve(64);

            auto q = w.query<const Position, Velocity>();
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel) {
                int my_swarm = e.has<DoctorBoid>() ? 1 : 0;
                const SwarmParams& params = (my_swarm == 1) ? config.doctor : config.normal;

                float sep_r_sq = params.separation_radius * params.separation_radius;
                float ali_r_sq = params.alignment_radius * params.alignment_radius;
                float coh_r_sq = params.cohesion_radius * params.cohesion_radius;

                grid.query_neighbors_fov(pos.x, pos.y, query_radius, neighbors, params.fov, vel.vx, vel.vy);

                Vector2 my_vel = {vel.vx, vel.vy};

                Vector2 sep = Vector2Zero();
                int sep_count = 0;
                Vector2 ali = Vector2Zero();
                int ali_count = 0;
                Vector2 coh = Vector2Zero();
                int coh_count = 0;

                for (const auto& qr : neighbors) {
                    const auto* ne = qr.entry;
                    if (ne->entity_id == e.id()) continue;
                    if (qr.dist_sq < 0.000001f) continue;

                    int ne_swarm = static_cast<int>(ne->swarm_type);

                    // Separation: repel from ALL nearby boids (cross-swarm), toroidal-aware
                    if (qr.dist_sq < sep_r_sq) {
                        float sdx, sdy;
                        displacement(ne->x, ne->y, pos.x, pos.y, toroidal, ww, wh, sdx, sdy);
                        Vector2 diff = {sdx, sdy};
                        sep = Vector2Add(sep, Vector2Scale(diff, 1.0f / qr.dist_sq));
                        sep_count++;
                    }

                    // Alignment: same swarm only
                    if (qr.dist_sq < ali_r_sq && ne_swarm == my_swarm) {
                        ali = Vector2Add(ali, {ne->vx, ne->vy});
                        ali_count++;
                    }

                    // Cohesion: same swarm only, accumulate displacements (toroidal-safe)
                    if (qr.dist_sq < coh_r_sq && ne_swarm == my_swarm) {
                        float cdx, cdy;
                        displacement(pos.x, pos.y, ne->x, ne->y, toroidal, ww, wh, cdx, cdy);
                        coh = Vector2Add(coh, {cdx, cdy});
                        coh_count++;
                    }
                }

                Vector2 force = Vector2Zero();

                if (sep_count > 0) {
                    sep = Vector2Scale(sep, 1.0f / sep_count);
                    Vector2 steer = steer_toward(sep, params.max_speed, my_vel, params.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, params.separation_weight));
                }

                if (ali_count > 0) {
                    ali = Vector2Scale(ali, 1.0f / ali_count);
                    Vector2 steer = steer_toward(ali, params.max_speed, my_vel, params.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, params.alignment_weight));
                }

                // Cohesion: coh IS the toward-center displacement (toroidal-safe)
                if (coh_count > 0) {
                    Vector2 toward_center = Vector2Scale(coh, 1.0f / coh_count);
                    Vector2 steer = steer_toward(toward_center, params.max_speed, my_vel, params.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, params.cohesion_weight));
                }

                // Chaotic noise injection (per-swarm noise_factor)
                if (params.noise_factor > 0.0f) {
                    static std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * PI);
                    float angle = angle_dist(sim_rng());
                    force.x += cosf(angle) * params.noise_factor;
                    force.y += sinf(angle) * params.noise_factor;
                }

                vel.vx += force.x * dt;
                vel.vy += force.y * dt;

                // Clamp velocity to per-swarm max_speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);
                if (speed > params.max_speed) {
                    v = Vector2Scale(v, params.max_speed / speed);
                    vel.vx = v.x;
                    vel.vy = v.y;
                }
            });
        });
}

void register_doctor_steering_system(flecs::world& world) {
    float max_speed = 180.0;
    float max_force = 180.0;
    
    float separation_radius = 25.0;
    float separation_weight = 4.5;

    float alignment_radius = 50.0;
    float alignment_weight = 4.5;
    
    float cohesion_radius = 50.0;
    float cohesion_weight = 4.5;
    
    float fov = 3.14;
    float noise_factor = 0.0;

    world.system("DoctorSteeringSystem")
        .kind(flecs::OnUpdate)
        .run([
            max_speed, max_force, separation_radius, separation_weight,
            alignment_radius, alignment_weight, cohesion_radius, cohesion_weight,
            fov, noise_factor
        ](flecs::iter& it) {
            flecs::world w = it.world();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            float dt = it.delta_time();

            auto q = w.query_builder<const Position, Velocity>().with<const DoctorBoid>().build();
            float query_radius = std::max({separation_radius,
                                            alignment_radius,
                                            cohesion_radius});
            float sep_r_sq = separation_radius * separation_radius;
            float ali_r_sq = alignment_radius * alignment_radius;
            float coh_r_sq = cohesion_radius * cohesion_radius;

            std::vector<SpatialGrid::QueryResult> neighbors;
            neighbors.reserve(64);  // Pre-allocate for typical neighbor count
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel) {
                // Query neighbors within the largest steering radius
                grid.query_neighbors_fov(pos.x, pos.y, query_radius, neighbors, fov, vel.vx, vel.vy);

                // Cache own swarm type once (avoid re-checking per neighbor)
                int my_swarm = e.has<DoctorBoid>() ? 1 : 0;
                Vector2 my_pos = {pos.x, pos.y};
                Vector2 my_vel = {vel.vx, vel.vy};

                // Separation accumulators (inverse-distance weighted, Model B)
                Vector2 sep = Vector2Zero();
                int sep_count = 0;
                // Alignment accumulators
                Vector2 ali = Vector2Zero();
                int ali_count = 0;
                // Cohesion accumulators
                Vector2 coh = Vector2Zero();
                int coh_count = 0;

                for (const auto& qr : neighbors) {
                    const auto* ne = qr.entry;
                    if (ne->entity_id == e.id()) continue; // skip self
                    if (qr.dist_sq < 0.000001f) continue; // skip overlapping

                    // Read from enriched entry instead of FLECS lookups
                    int ne_swarm = static_cast<int>(ne->swarm_type);

                    // Separation: repel from ALL nearby boids (cross-swarm)
                    // Model B Section 2.2: f_j = diff / d_ij^2 (inverse-distance weighted)
                    if (qr.dist_sq < sep_r_sq) {
                        Vector2 diff = Vector2Subtract(my_pos, {ne->x, ne->y});
                        sep = Vector2Add(sep, Vector2Scale(diff, 1.0f / qr.dist_sq));
                        sep_count++;
                    }

                    // Alignment: same swarm only (independent flocking)
                    if (qr.dist_sq < ali_r_sq && ne_swarm == my_swarm) {
                        ali = Vector2Add(ali, {ne->vx, ne->vy});
                        ali_count++;
                    }

                    // Cohesion: same swarm only (independent flocking)
                    if (qr.dist_sq < coh_r_sq && ne_swarm == my_swarm) {
                        coh = Vector2Add(coh, {ne->x, ne->y});
                        coh_count++;
                    }
                }

                Vector2 force = Vector2Zero();

                // --- Separation: Model B (Shiffman) ---
                // Average, compute desired velocity, truncate per-behavior
                if (sep_count > 0) {
                    sep = Vector2Scale(sep, 1.0f / sep_count);
                    Vector2 steer = steer_toward(sep, max_speed, my_vel, max_force);
                    force = Vector2Add(force, Vector2Scale(steer, separation_weight));
                }

                // --- Alignment: Model B (Shiffman) with per-behavior truncation ---
                if (ali_count > 0) {
                    ali = Vector2Scale(ali, 1.0f / ali_count);
                    Vector2 steer = steer_toward(ali, max_speed, my_vel, max_force);
                    force = Vector2Add(force, Vector2Scale(steer, alignment_weight));
                }

                // --- Cohesion: Model B (Shiffman) with per-behavior truncation ---
                if (coh_count > 0) {
                    coh = Vector2Scale(coh, 1.0f / coh_count);
                    Vector2 toward_center = Vector2Subtract(coh, my_pos);
                    Vector2 steer = steer_toward(toward_center, max_speed, my_vel, max_force);
                    force = Vector2Add(force, Vector2Scale(steer, cohesion_weight));
                }

                // B3 chaotic noise injection (only when noise_factor > 0)
                if (noise_factor > 0.0f) {
                    static std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * PI);
                    float angle = angle_dist(sim_rng());
                    force.x += cosf(angle) * noise_factor;
                    force.y += sinf(angle) * noise_factor;
                }

                // Apply force to velocity
                vel.vx += force.x * dt;
                vel.vy += force.y * dt;

                // Clamp velocity to max_speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);
                if (speed > max_speed) {
                    v = Vector2Scale(v, max_speed / speed);
                    vel.vx = v.x;
                    vel.vy = v.y;
                }
            });
        });
}

void register_movement_system(flecs::world& world) {
    world.system("MovementSystem")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            float dt = it.delta_time();

            auto q = w.query<Position, Velocity, Heading>();
            q.each([&](flecs::entity e, Position& pos, Velocity& vel, Heading& heading) {
                // Apply velocity to position
                pos.x += vel.vx * dt;
                pos.y += vel.vy * dt;

                if (config.wall_bounce) {
                    // Bounce off walls (reflect velocity)
                    if (pos.x < 0.0f) {
                        pos.x = - pos.x;
                        vel.vx = - vel.vx;
                    }
                    if (pos.x >= config.world_width) {
                        pos.x = 2 * config.world_width - pos.x;
                        vel.vx = - vel.vx;
                    }
                    if (pos.y < 0.0f) {
                        pos.y = - pos.y;
                        vel.vy = - vel.vy;
                    }
                    if (pos.y >= config.world_height) {
                        pos.y = 2 * config.world_height - pos.y;
                        vel.vy = - vel.vy;
                    }
                } else {
                    // Wrap around world bounds
                    if (pos.x < 0.0f) pos.x += config.world_width;
                    if (pos.x >= config.world_width) pos.x -= config.world_width;
                    if (pos.y < 0.0f) pos.y += config.world_height;
                    if (pos.y >= config.world_height) pos.y -= config.world_height;
                }

                // Compute actual speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);

                // Enforce per-swarm minimum speed
                const SwarmParams& params = e.has<DoctorBoid>() ? config.doctor : config.normal;
                if (speed > 0.001f && speed < params.min_speed && params.min_speed <= params.max_speed) {
                    v = Vector2Scale(v, params.min_speed / speed);
                    vel.vx = v.x;
                    vel.vy = v.y;
                }

                // Update heading based on velocity
                if (speed > 0.01f) {
                    heading.angle = std::atan2(vel.vy, vel.vx);
                }
            });
        });
}
