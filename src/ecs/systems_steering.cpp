#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include <flecs.h>
#include <cmath>
#include <algorithm>
#include <vector>

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

            auto q = w.query<const Position, const Velocity, const Alive>();
            q.each([&grid](flecs::entity e, const Position& pos, const Velocity& vel, const Alive&) {
                uint8_t swarm_type = e.has<NormalBoid>() ? 0
                                   : e.has<DoctorBoid>() ? 1 : 2;
                uint8_t flags = SpatialGrid::FLAG_ALIVE;
                if (e.has<Infected>()) flags |= SpatialGrid::FLAG_INFECTED;
                if (e.has<Male>())     flags |= SpatialGrid::FLAG_MALE;

                grid.insert(e.id(), pos.x, pos.y, vel.vx, vel.vy, swarm_type, flags);
            });
        });
}

// ============================================================
// OnUpdate Phase: Steering and Movement
// ============================================================

void register_antivax_steering_system(flecs::world& world) {
    world.system("AntivaxSteeringSystem")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            float dt = it.delta_time();

            // Only process AntivaxBoid entities (primary swarm tag)
            auto q = w.query<const Position, Velocity, const AntivaxBoid, const Alive>();
            std::vector<SpatialGrid::QueryResult> neighbors;
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel, const AntivaxBoid&, const Alive&) {
                // Query for doctors within visual range
                grid.query_neighbors(pos.x, pos.y, config.antivax_repulsion_radius, neighbors);

                // Model B: inverse-distance weighted repulsion from doctors
                float repulsion_x = 0.0f, repulsion_y = 0.0f;
                int doctor_count = 0;

                for (const auto& qr : neighbors) {
                    const auto* ne = qr.entry;
                    if (ne->entity_id == e.id()) continue;
                    if (qr.dist_sq < 0.000001f) continue;

                    // Use enriched entry: only doctors trigger repulsion
                    if (ne->swarm_type != 1) continue;
                    if (!(ne->flags & SpatialGrid::FLAG_ALIVE)) continue;

                    float dist = std::sqrt(qr.dist_sq);
                    float dx = pos.x - ne->x;
                    float dy = pos.y - ne->y;

                    // Inverse-distance weighting: normalize(diff) / distance
                    repulsion_x += (dx / dist) / dist;
                    repulsion_y += (dy / dist) / dist;
                    doctor_count++;
                }

                if (doctor_count > 0) {
                    // Average, then Reynolds steering: desired - current
                    repulsion_x /= static_cast<float>(doctor_count);
                    repulsion_y /= static_cast<float>(doctor_count);
                    float rep_mag = std::sqrt(repulsion_x * repulsion_x + repulsion_y * repulsion_y);

                    if (rep_mag > 0.001f) {
                        float desired_vx = (repulsion_x / rep_mag) * config.max_speed;
                        float desired_vy = (repulsion_y / rep_mag) * config.max_speed;
                        float steer_x = desired_vx - vel.vx;
                        float steer_y = desired_vy - vel.vy;

                        // Per-behavior truncation to max_force
                        float steer_mag = std::sqrt(steer_x * steer_x + steer_y * steer_y);
                        if (steer_mag > config.max_force) {
                            float scale = config.max_force / steer_mag;
                            steer_x *= scale;
                            steer_y *= scale;
                        }

                        float force_x = steer_x * config.antivax_repulsion_weight;
                        float force_y = steer_y * config.antivax_repulsion_weight;

                        vel.vx += force_x * dt;
                        vel.vy += force_y * dt;

                        // Clamp velocity to max_speed
                        float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
                        if (speed > config.max_speed) {
                            float scale = config.max_speed / speed;
                            vel.vx *= scale;
                            vel.vy *= scale;
                        }
                    }
                }
            });
        });
}

void register_steering_system(flecs::world& world) {
    world.system("SteeringSystem")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            float dt = it.delta_time();

            auto q = w.query<const Position, Velocity, const Alive>();
            float query_radius = std::max({config.separation_radius,
                                            config.alignment_radius,
                                            config.cohesion_radius});
            float sep_r_sq = config.separation_radius * config.separation_radius;
            float ali_r_sq = config.alignment_radius * config.alignment_radius;
            float coh_r_sq = config.cohesion_radius * config.cohesion_radius;

            std::vector<SpatialGrid::QueryResult> neighbors;
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel, const Alive&) {
                // Query neighbors within the largest steering radius
                grid.query_neighbors(pos.x, pos.y, query_radius, neighbors);

                // Cache own swarm type once (avoid re-checking per neighbor)
                int my_swarm = e.has<NormalBoid>() ? 0 : e.has<DoctorBoid>() ? 1 : 2;

                // Separation accumulators (inverse-distance weighted, Model B)
                float sep_x = 0.0f, sep_y = 0.0f;
                int sep_count = 0;
                // Alignment accumulators
                float ali_vx = 0.0f, ali_vy = 0.0f;
                int ali_count = 0;
                // Cohesion accumulators
                float coh_x = 0.0f, coh_y = 0.0f;
                int coh_count = 0;

                for (const auto& qr : neighbors) {
                    const auto* ne = qr.entry;
                    if (ne->entity_id == e.id()) continue; // skip self
                    if (qr.dist_sq < 0.000001f) continue; // skip overlapping

                    if (!(ne->flags & SpatialGrid::FLAG_ALIVE)) continue;

                    // Read from enriched entry instead of FLECS lookups
                    int ne_swarm = static_cast<int>(ne->swarm_type);
                    bool is_same_swarm = (my_swarm == ne_swarm);

                    // Separation: repel from ALL nearby boids (cross-swarm)
                    // Model B: normalize(diff) / distance — inverse-distance weighting
                    if (qr.dist_sq < sep_r_sq) {
                        float dist = std::sqrt(qr.dist_sq);
                        float dx = pos.x - ne->x;
                        float dy = pos.y - ne->y;
                        sep_x += (dx / dist) / dist;
                        sep_y += (dy / dist) / dist;
                        sep_count++;
                    }

                    // Alignment: match velocity of SAME-SWARM nearby boids only
                    if (is_same_swarm && qr.dist_sq < ali_r_sq) {
                        ali_vx += ne->vx;
                        ali_vy += ne->vy;
                        ali_count++;
                    }

                    // Cohesion: steer toward center of mass of SAME-SWARM only
                    if (is_same_swarm && qr.dist_sq < coh_r_sq) {
                        coh_x += ne->x;
                        coh_y += ne->y;
                        coh_count++;
                    }
                }

                float force_x = 0.0f, force_y = 0.0f;

                // --- Separation: Model B (Shiffman) ---
                // Average, compute desired velocity, truncate per-behavior
                if (sep_count > 0) {
                    sep_x /= static_cast<float>(sep_count);
                    sep_y /= static_cast<float>(sep_count);
                    float sep_mag = std::sqrt(sep_x * sep_x + sep_y * sep_y);
                    if (sep_mag > 0.001f) {
                        // desired = normalize(steer) * max_speed
                        float desired_vx = (sep_x / sep_mag) * config.max_speed;
                        float desired_vy = (sep_y / sep_mag) * config.max_speed;
                        float steer_x = desired_vx - vel.vx;
                        float steer_y = desired_vy - vel.vy;
                        // Per-behavior truncation to max_force
                        float steer_mag = std::sqrt(steer_x * steer_x + steer_y * steer_y);
                        if (steer_mag > config.max_force) {
                            float scale = config.max_force / steer_mag;
                            steer_x *= scale;
                            steer_y *= scale;
                        }
                        force_x += steer_x * config.separation_weight;
                        force_y += steer_y * config.separation_weight;
                    }
                }

                // --- Alignment: Model B (Shiffman) with per-behavior truncation ---
                if (ali_count > 0) {
                    ali_vx /= static_cast<float>(ali_count);
                    ali_vy /= static_cast<float>(ali_count);
                    float ali_mag = std::sqrt(ali_vx * ali_vx + ali_vy * ali_vy);
                    if (ali_mag > 0.001f) {
                        float desired_vx = (ali_vx / ali_mag) * config.max_speed;
                        float desired_vy = (ali_vy / ali_mag) * config.max_speed;
                        float steer_x = desired_vx - vel.vx;
                        float steer_y = desired_vy - vel.vy;
                        // Per-behavior truncation to max_force
                        float steer_mag = std::sqrt(steer_x * steer_x + steer_y * steer_y);
                        if (steer_mag > config.max_force) {
                            float scale = config.max_force / steer_mag;
                            steer_x *= scale;
                            steer_y *= scale;
                        }
                        force_x += steer_x * config.alignment_weight;
                        force_y += steer_y * config.alignment_weight;
                    }
                }

                // --- Cohesion: Model B (Shiffman) with per-behavior truncation ---
                if (coh_count > 0) {
                    coh_x /= static_cast<float>(coh_count);
                    coh_y /= static_cast<float>(coh_count);
                    float dx = coh_x - pos.x;
                    float dy = coh_y - pos.y;
                    float mag = std::sqrt(dx * dx + dy * dy);
                    if (mag > 0.001f) {
                        float desired_vx = (dx / mag) * config.max_speed;
                        float desired_vy = (dy / mag) * config.max_speed;
                        float steer_x = desired_vx - vel.vx;
                        float steer_y = desired_vy - vel.vy;
                        // Per-behavior truncation to max_force
                        float steer_mag = std::sqrt(steer_x * steer_x + steer_y * steer_y);
                        if (steer_mag > config.max_force) {
                            float scale = config.max_force / steer_mag;
                            steer_x *= scale;
                            steer_y *= scale;
                        }
                        force_x += steer_x * config.cohesion_weight;
                        force_y += steer_y * config.cohesion_weight;
                    }
                }

                // Apply force to velocity
                vel.vx += force_x * dt;
                vel.vy += force_y * dt;

                // Clamp velocity to max_speed
                float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
                if (speed > config.max_speed) {
                    float scale = config.max_speed / speed;
                    vel.vx *= scale;
                    vel.vy *= scale;
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

                // Wrap around world bounds
                if (pos.x < 0.0f) pos.x += config.world_width;
                if (pos.x >= config.world_width) pos.x -= config.world_width;
                if (pos.y < 0.0f) pos.y += config.world_height;
                if (pos.y >= config.world_height) pos.y -= config.world_height;

                // Compute actual speed
                float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);

                // Enforce minimum speed — prevents boids from stalling
                // Guard: only enforce if min_speed < max_speed (slider edge case)
                if (speed > 0.001f && speed < config.min_speed && config.min_speed <= config.max_speed) {
                    float scale = config.min_speed / speed;
                    vel.vx *= scale;
                    vel.vy *= scale;
                }

                // Update heading based on velocity
                if (speed > 0.01f) {
                    heading.angle = std::atan2(vel.vy, vel.vx);
                }
            });
        });
}
