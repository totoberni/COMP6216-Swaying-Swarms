#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "render_state.h"
#include "render/render_config.h"
#include <flecs.h>
#include <cmath>
#include <algorithm>

// ============================================================
// PreUpdate Phase: Spatial Grid Rebuild
// ============================================================

void register_rebuild_grid_system(flecs::world& world) {
    world.system("RebuildGridSystem")
        .kind(flecs::PreUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SpatialGrid& grid = w.get_mut<SpatialGrid>();

            grid.clear();

            auto q = w.query<const Position, const Alive>();
            q.each([&grid](flecs::entity e, const Position& pos, const Alive&) {
                grid.insert(e.id(), pos.x, pos.y);
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

            auto q = w.query<const Position, Velocity, const Alive>();
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel, const Alive&) {
                // Query neighbors within the largest steering radius
                float query_radius = std::max({config.separation_radius,
                                                config.alignment_radius,
                                                config.cohesion_radius});
                auto neighbors = grid.query_neighbors(pos.x, pos.y, query_radius);

                float sep_x = 0.0f, sep_y = 0.0f;
                float ali_vx = 0.0f, ali_vy = 0.0f;
                float coh_x = 0.0f, coh_y = 0.0f;
                int sep_count = 0, ali_count = 0, coh_count = 0;

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue; // skip self
                    if (dist < 0.001f) continue; // skip overlapping

                    // Look up neighbor's components
                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Position>() || !ne.has<Velocity>()) continue;

                    const Position& npos = ne.get<Position>();
                    const Velocity& nvel = ne.get<Velocity>();

                    float dx = pos.x - npos.x;
                    float dy = pos.y - npos.y;

                    // Separation: repel from nearby boids
                    if (dist < config.separation_radius) {
                        sep_x += dx / dist;
                        sep_y += dy / dist;
                        sep_count++;
                    }

                    // Alignment: match velocity of nearby boids
                    if (dist < config.alignment_radius) {
                        ali_vx += nvel.vx;
                        ali_vy += nvel.vy;
                        ali_count++;
                    }

                    // Cohesion: steer toward center of mass
                    if (dist < config.cohesion_radius) {
                        coh_x += npos.x;
                        coh_y += npos.y;
                        coh_count++;
                    }
                }

                float force_x = 0.0f, force_y = 0.0f;

                // Average and apply separation force
                if (sep_count > 0) {
                    sep_x /= static_cast<float>(sep_count);
                    sep_y /= static_cast<float>(sep_count);
                    force_x += sep_x * config.separation_weight;
                    force_y += sep_y * config.separation_weight;
                }

                // Average and apply alignment force (desired = avg neighbor vel)
                if (ali_count > 0) {
                    ali_vx /= static_cast<float>(ali_count);
                    ali_vy /= static_cast<float>(ali_count);
                    force_x += (ali_vx - vel.vx) * config.alignment_weight;
                    force_y += (ali_vy - vel.vy) * config.alignment_weight;
                }

                // Average and apply cohesion force (steer toward center)
                if (coh_count > 0) {
                    coh_x /= static_cast<float>(coh_count);
                    coh_y /= static_cast<float>(coh_count);
                    force_x += (coh_x - pos.x) * config.cohesion_weight;
                    force_y += (coh_y - pos.y) * config.cohesion_weight;
                }

                // Clamp total force to max_force
                float force_mag = std::sqrt(force_x * force_x + force_y * force_y);
                if (force_mag > config.max_force) {
                    float scale = config.max_force / force_mag;
                    force_x *= scale;
                    force_y *= scale;
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
    world.system<Position, const Velocity, Heading>("MovementSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::iter& it, size_t index, Position& pos, const Velocity& vel, Heading& heading) {
            const SimConfig& config = it.world().get<SimConfig>();
            float dt = it.delta_time();

            // Apply velocity to position
            pos.x += vel.vx * dt;
            pos.y += vel.vy * dt;

            // Wrap around world bounds
            if (pos.x < 0.0f) pos.x += config.world_width;
            if (pos.x >= config.world_width) pos.x -= config.world_width;
            if (pos.y < 0.0f) pos.y += config.world_height;
            if (pos.y >= config.world_height) pos.y -= config.world_height;

            // Update heading based on velocity
            float speed = vel.vx * vel.vx + vel.vy * vel.vy;
            if (speed > 0.01f) {
                heading.angle = std::atan2(vel.vy, vel.vx);
            }
        });
}

// ============================================================
// PostUpdate Phase: Collisions and Behavior (stubs for Phase 10)
// ============================================================

void register_collision_system(flecs::world& world) {
    world.system<const Position, const Alive>("CollisionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Position& pos, const Alive&) {
            // Stub: Phase 10 will implement infection/cure/reproduction triggers
        });
}

void register_infection_system(flecs::world& world) {
    world.system<InfectionState, const Alive>("InfectionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, InfectionState& infection, const Alive&) {
            // Stub: Phase 10 — update infection timers, trigger death
        });
}

void register_cure_system(flecs::world& world) {
    world.system<const Alive>("CureSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Alive&) {
            // Stub: Phase 10 — doctor cures infected boids
        });
}

void register_reproduction_system(flecs::world& world) {
    world.system<const Position, ReproductionCooldown, const Alive>("ReproductionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::iter& it, size_t index, const Position& pos, ReproductionCooldown& cooldown, const Alive&) {
            float dt = it.delta_time();
            if (cooldown.cooldown > 0.0f) {
                cooldown.cooldown -= dt;
            }
        });
}

void register_death_system(flecs::world& world) {
    world.system<const Health, const InfectionState, const Alive>("DeathSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Health& health, const InfectionState& infection, const Alive&) {
            // Stub: Phase 10 — remove Alive tag on death
        });
}

void register_doctor_promotion_system(flecs::world& world) {
    world.system<const Alive>("DoctorPromotionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Alive&) {
            // Stub: Phase 10 — promote adult normal boids to doctors
        });
}

// ============================================================
// OnStore Phase: Stats + Render Sync
// ============================================================

void register_render_sync_system(flecs::world& world) {
    world.system("RenderSyncSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            RenderState& rs = w.get_mut<RenderState>();

            // Clear previous frame data
            rs.boids.clear();

            // Copy current stats
            rs.stats = w.get<SimStats>();

            // Build render data for all alive boids
            auto q = w.query<const Position, const Velocity, const Heading, const Alive>();
            q.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                        const Heading& heading, const Alive&) {
                BoidRenderData brd;
                brd.x = pos.x;
                brd.y = pos.y;
                brd.angle = heading.angle;
                brd.is_doctor = e.has<DoctorBoid>();

                // Color: green for normal, orange for doctor, red if infected
                if (e.has<Infected>()) {
                    brd.color = RenderConfig::COLOR_INFECTED;
                } else if (brd.is_doctor) {
                    brd.color = RenderConfig::COLOR_DOCTOR;
                } else {
                    brd.color = RenderConfig::COLOR_NORMAL;
                }

                brd.radius = brd.is_doctor ? config.r_interact_doctor : config.r_interact_normal;
                rs.boids.push_back(brd);
            });
        });
}

// ============================================================
// System Registration
// ============================================================

void register_all_systems(flecs::world& world) {
    // PreUpdate
    register_rebuild_grid_system(world);

    // OnUpdate
    register_steering_system(world);
    register_movement_system(world);

    // PostUpdate
    register_collision_system(world);
    register_infection_system(world);
    register_cure_system(world);
    register_reproduction_system(world);
    register_death_system(world);
    register_doctor_promotion_system(world);

    // OnStore
    register_render_sync_system(world);
}
