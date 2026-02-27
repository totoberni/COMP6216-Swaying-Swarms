#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include <flecs.h>
#include <cmath>
#include <raymath.h>
#include <algorithm>
#include <vector>

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
                uint8_t swarm_type = e.has<NormalBoid>() ? 0
                                   : e.has<DoctorBoid>() ? 1 : 2;
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

            auto q = w.query<const Position, Velocity>();
            float query_radius = std::max({config.separation_radius,
                                            config.alignment_radius,
                                            config.cohesion_radius});
            float sep_r_sq = config.separation_radius * config.separation_radius;
            float ali_r_sq = config.alignment_radius * config.alignment_radius;
            float coh_r_sq = config.cohesion_radius * config.cohesion_radius;

            std::vector<SpatialGrid::QueryResult> neighbors;
            neighbors.reserve(64);  // Pre-allocate for typical neighbor count
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel) {
                // Query neighbors within the largest steering radius
                grid.query_neighbors_fov(pos.x, pos.y, query_radius, neighbors, config.fov, vel.vx, vel.vy);

                // Cache own swarm type once (avoid re-checking per neighbor)
                int my_swarm = e.has<NormalBoid>() ? 0 : e.has<DoctorBoid>() ? 1 : 2;
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

                    // Alignment: match velocity of nearby boids only
                    if (qr.dist_sq < ali_r_sq) {
                        ali = Vector2Add(ali, {ne->vx, ne->vy});
                        ali_count++;
                    }

                    // Cohesion: steer toward center of mass
                    if (qr.dist_sq < coh_r_sq) {
                        coh = Vector2Add(coh, {ne->x, ne->y});
                        coh_count++;
                    }
                }

                Vector2 force = Vector2Zero();

                // --- Separation: Model B (Shiffman) ---
                // Average, compute desired velocity, truncate per-behavior
                if (sep_count > 0) {
                    sep = Vector2Scale(sep, 1.0f / sep_count);
                    Vector2 steer = steer_toward(sep, config.max_speed, my_vel, config.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, config.separation_weight));
                }

                // --- Alignment: Model B (Shiffman) with per-behavior truncation ---
                if (ali_count > 0) {
                    ali = Vector2Scale(ali, 1.0f / ali_count);
                    Vector2 steer = steer_toward(ali, config.max_speed, my_vel, config.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, config.alignment_weight));
                }

                // --- Cohesion: Model B (Shiffman) with per-behavior truncation ---
                if (coh_count > 0) {
                    coh = Vector2Scale(coh, 1.0f / coh_count);
                    Vector2 toward_center = Vector2Subtract(coh, my_pos);
                    Vector2 steer = steer_toward(toward_center, config.max_speed, my_vel, config.max_force);
                    force = Vector2Add(force, Vector2Scale(steer, config.cohesion_weight));
                }

                // Apply force to velocity
                vel.vx += force.x * dt;
                vel.vy += force.y * dt;

                // Clamp velocity to max_speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);
                if (speed > config.max_speed) {
                    v = Vector2Scale(v, config.max_speed / speed);
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
            q.each([&](Position& pos, Velocity& vel, Heading& heading) {
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

                // Enforce minimum speed — prevents boids from stalling
                // Guard: only enforce if min_speed < max_speed (slider edge case)
                if (speed > 0.001f && speed < config.min_speed && config.min_speed <= config.max_speed) {
                    v = Vector2Scale(v, config.min_speed / speed);
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
