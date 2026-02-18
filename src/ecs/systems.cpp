#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "render_state.h"
#include "render/render_config.h"
#include "sim/aging.h"
#include "sim/death.h"
#include "sim/infection.h"
#include "sim/cure.h"
#include "sim/reproduction.h"
#include "sim/promotion.h"
#include "sim/rng.h"
#include <flecs.h>
#include <cmath>
#include <algorithm>

namespace {
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 2.0f * PI;
}

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
            q.each([&](flecs::entity e, const Position& pos, Velocity& vel, const AntivaxBoid&, const Alive&) {
                // Query for doctors within visual range
                auto neighbors = grid.query_neighbors(pos.x, pos.y, config.antivax_repulsion_radius);

                float repulsion_x = 0.0f, repulsion_y = 0.0f;
                int doctor_count = 0;

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;
                    if (dist < 0.001f) continue; // skip overlapping

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only repel from doctors
                    if (!ne.has<DoctorBoid>()) continue;

                    const Position& npos = ne.get<Position>();
                    float dx = pos.x - npos.x;
                    float dy = pos.y - npos.y;

                    // Add repulsion force (stronger when closer)
                    repulsion_x += dx / dist;
                    repulsion_y += dy / dist;
                    doctor_count++;
                }

                // Apply repulsion force if any doctors detected
                if (doctor_count > 0) {
                    repulsion_x /= static_cast<float>(doctor_count);
                    repulsion_y /= static_cast<float>(doctor_count);

                    float force_x = repulsion_x * config.antivax_repulsion_weight;
                    float force_y = repulsion_y * config.antivax_repulsion_weight;

                    // Clamp repulsion force to max_force
                    float force_mag = std::sqrt(force_x * force_x + force_y * force_y);
                    if (force_mag > config.max_force) {
                        float scale = config.max_force / force_mag;
                        force_x *= scale;
                        force_y *= scale;
                    }

                    // Apply force to velocity (ADDITIVE to existing velocity)
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

                    // Determine if neighbor is same swarm (for alignment/cohesion)
                    bool is_same_swarm = (e.has<NormalBoid>() && ne.has<NormalBoid>()) ||
                                         (e.has<DoctorBoid>() && ne.has<DoctorBoid>()) ||
                                         (e.has<AntivaxBoid>() && ne.has<AntivaxBoid>());

                    // Separation: repel from ALL nearby boids (cross-swarm)
                    if (dist < config.separation_radius) {
                        sep_x += dx / dist;
                        sep_y += dy / dist;
                        sep_count++;
                    }

                    // Alignment: match velocity of SAME-SWARM nearby boids only
                    if (is_same_swarm && dist < config.alignment_radius) {
                        ali_vx += nvel.vx;
                        ali_vy += nvel.vy;
                        ali_count++;
                    }

                    // Cohesion: steer toward center of mass of SAME-SWARM only
                    if (is_same_swarm && dist < config.cohesion_radius) {
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

                // Average and apply alignment force (desired = normalize(avg vel) * max_speed)
                if (ali_count > 0) {
                    ali_vx /= static_cast<float>(ali_count);
                    ali_vy /= static_cast<float>(ali_count);
                    float ali_mag = std::sqrt(ali_vx * ali_vx + ali_vy * ali_vy);
                    if (ali_mag > 0.001f) {
                        float desired_vx = (ali_vx / ali_mag) * config.max_speed;
                        float desired_vy = (ali_vy / ali_mag) * config.max_speed;
                        force_x += (desired_vx - vel.vx) * config.alignment_weight;
                        force_y += (desired_vy - vel.vy) * config.alignment_weight;
                    }
                }

                // Average and apply cohesion force (desired = normalize(to COM) * max_speed)
                if (coh_count > 0) {
                    coh_x /= static_cast<float>(coh_count);
                    coh_y /= static_cast<float>(coh_count);
                    float dx = coh_x - pos.x;
                    float dy = coh_y - pos.y;
                    float mag = std::sqrt(dx * dx + dy * dy);
                    if (mag > 0.001f) {
                        float desired_vx = (dx / mag) * config.max_speed;
                        float desired_vy = (dy / mag) * config.max_speed;
                        force_x += (desired_vx - vel.vx) * config.cohesion_weight;
                        force_y += (desired_vy - vel.vy) * config.cohesion_weight;
                    }
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
    world.system<Position, Velocity, Heading>("MovementSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::iter& it, size_t index, Position& pos, Velocity& vel, Heading& heading) {
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
}

// ============================================================
// PostUpdate Phase: Aging and Timers
// ============================================================

void register_aging_system(flecs::world& world) {
    world.system("AgingSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            float dt = it.delta_time();

            // Age all alive entities
            auto q_age = w.query<Health, const Alive>();
            q_age.each([dt](Health& health, const Alive&) {
                age_entity(health.age, dt);
            });

            // Tick infection timers
            auto q_infected = w.query<InfectionState, const Infected, const Alive>();
            q_infected.each([dt](InfectionState& infection, const Infected&, const Alive&) {
                tick_infection(infection.time_infected, dt);
            });
        });
}

// ============================================================
// PostUpdate Phase: Collisions and Behavior
// ============================================================

void register_collision_system(flecs::world& world) {
    // Collision system stub reserved for Phase 11 extensions.
    // Currently all collision behavior is handled by infection and reproduction systems.
    // This system can be extended in the future for direct boid-boid collision responses.
    (void)world;  // Suppress unused parameter warning
}

void register_infection_system(flecs::world& world) {
    world.system("InfectionSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            std::mt19937& rng = sim_rng();

            w.defer_begin();

            // Process Normal Boids
            auto q_normal = w.query<const Position, const NormalBoid, const Alive>();
            q_normal.each([&](flecs::entity e, const Position& pos, const NormalBoid&, const Alive&) {
                bool is_infected = e.has<Infected>();

                // Only infected boids can spread infection
                if (!is_infected) return;

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_normal * config.debuff_r_interact_normal_infected;

                // Query neighbors within effective interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only infect same swarm type (NormalBoid -> NormalBoid)
                    if (!ne.has<NormalBoid>()) continue;

                    // Skip if already infected
                    if (ne.has<Infected>()) continue;

                    // Try to infect
                    if (try_infect(config.p_infect_normal, rng)) {
                        ne.add<Infected>();
                        ne.set(InfectionState{0.0f, config.t_death});
                    }
                }
            });

            // Process Doctor Boids
            auto q_doctor = w.query<const Position, const DoctorBoid, const Alive>();
            q_doctor.each([&](flecs::entity e, const Position& pos, const DoctorBoid&, const Alive&) {
                bool is_infected = e.has<Infected>();

                // Only infected boids can spread infection
                if (!is_infected) return;

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_doctor * config.debuff_r_interact_doctor_infected;

                // Query neighbors within effective interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only infect same swarm type (DoctorBoid -> DoctorBoid)
                    if (!ne.has<DoctorBoid>()) continue;

                    // Skip if already infected
                    if (ne.has<Infected>()) continue;

                    // Try to infect
                    if (try_infect(config.p_infect_doctor, rng)) {
                        ne.add<Infected>();
                        ne.set(InfectionState{0.0f, config.t_death});
                    }
                }
            });

            // Process Antivax Boids (same-swarm infection only)
            auto q_antivax = w.query<const Position, const AntivaxBoid, const Alive>();
            q_antivax.each([&](flecs::entity e, const Position& pos, const AntivaxBoid&, const Alive&) {
                bool is_infected = e.has<Infected>();
                if (!is_infected) return;

                float effective_r_interact = config.r_interact_normal * config.debuff_r_interact_normal_infected;
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;
                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;
                    if (!ne.has<AntivaxBoid>()) continue;  // Same-swarm only
                    if (ne.has<Infected>()) continue;

                    if (try_infect(config.p_infect_normal, rng)) {
                        ne.add<Infected>();
                        ne.set(InfectionState{0.0f, config.t_death});
                    }
                }
            });

            w.defer_end();
        });
}

void register_cure_system(flecs::world& world) {
    world.system("CureSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            std::mt19937& rng = sim_rng();

            w.defer_begin();

            // Only doctors can cure
            auto q_doctor = w.query<const Position, const DoctorBoid, const Alive>();
            q_doctor.each([&](flecs::entity e, const Position& pos, const DoctorBoid&, const Alive&) {
                // Check if doctor is infected for debuff calculation
                bool doctor_infected = e.has<Infected>();

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_doctor;
                if (doctor_infected) {
                    effective_r_interact *= config.debuff_r_interact_doctor_infected;
                }

                // Query neighbors within effective doctor interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    // Contract: doctors cannot cure themselves — only another doctor can cure them
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Skip if not infected
                    if (!ne.has<Infected>()) continue;

                    // Calculate effective cure probability (debuffed if doctor is infected)
                    float effective_p_cure = config.p_cure;
                    if (doctor_infected) {
                        effective_p_cure *= config.debuff_p_cure_infected;
                    }

                    // Try to cure (doctors cure ANY infected boid, including other doctors)
                    if (try_cure(effective_p_cure, rng)) {
                        ne.remove<Infected>();
                        // Reset infection timer
                        if (ne.has<InfectionState>()) {
                            InfectionState& inf = ne.get_mut<InfectionState>();
                            inf.time_infected = 0.0f;
                        }
                    }
                }
            });

            w.defer_end();
        });
}

void register_reproduction_system(flecs::world& world) {
    world.system("ReproductionSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            const SpatialGrid& grid = w.get<SpatialGrid>();
            SimStats& stats = w.get_mut<SimStats>();
            std::mt19937& rng = sim_rng();
            float dt = it.delta_time();

            // First, update cooldowns for all alive boids
            auto q_cooldown = w.query<ReproductionCooldown, const Alive>();
            q_cooldown.each([dt](ReproductionCooldown& cooldown, const Alive&) {
                if (cooldown.cooldown > 0.0f) {
                    cooldown.cooldown -= dt;
                }
            });

            w.defer_begin();

            // Process Normal Boid reproduction
            auto q_normal = w.query<const Position, const Velocity, ReproductionCooldown, const NormalBoid, const Alive>();
            q_normal.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                              ReproductionCooldown& cooldown, const NormalBoid&, const Alive&) {
                // Skip if on cooldown
                if (cooldown.cooldown > 0.0f) return;

                bool is_infected = e.has<Infected>();

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_normal;
                if (is_infected) {
                    effective_r_interact *= config.debuff_r_interact_normal_infected;
                }

                // Query neighbors within effective interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only reproduce with same swarm type
                    if (!ne.has<NormalBoid>()) continue;

                    // Check opposite sex requirement
                    bool parent_is_male = e.has<Male>();
                    bool neighbor_is_male = ne.has<Male>();
                    if (parent_is_male == neighbor_is_male) continue; // Same sex, skip

                    // Check neighbor's cooldown
                    if (!ne.has<ReproductionCooldown>()) continue;
                    const ReproductionCooldown& ncooldown = ne.get<ReproductionCooldown>();
                    if (ncooldown.cooldown > 0.0f) continue;

                    // Calculate effective reproduction probability (debuffed if infected)
                    float effective_p_offspring = config.p_offspring_normal;
                    if (is_infected) {
                        effective_p_offspring *= config.debuff_p_offspring_normal_infected;
                    }

                    // Try to reproduce
                    if (!try_reproduce(effective_p_offspring, rng)) continue;

                    // Calculate offspring count
                    int count = offspring_count(config.offspring_mean_normal,
                                               config.offspring_stddev_normal, rng);

                    if (count <= 0) continue;

                    // Get neighbor position for midpoint calculation
                    const Position& npos = ne.get<Position>();
                    float spawn_x = (pos.x + npos.x) / 2.0f;
                    float spawn_y = (pos.y + npos.y) / 2.0f;

                    // Check if both parents are infected
                    bool neighbor_infected = ne.has<Infected>();
                    bool child_infected = false;
                    if (is_infected && neighbor_infected) {
                        // Child gets contagion from ONE parent only
                        child_infected = try_infect(config.p_infect_normal, rng);
                    }

                    // Spawn offspring
                    std::uniform_real_distribution<float> dist_angle(0.0f, TWO_PI);
                    std::uniform_real_distribution<float> dist_speed(0.0f, config.max_speed * 0.5f);
                    std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);

                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = dist_speed(rng);

                        auto child = w.entity()
                            .add<NormalBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        // Assign sex
                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        // Infect child if applicable
                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    // Set cooldown for both parents
                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    // Update stats
                    stats.newborns_total += count;
                    stats.newborns_normal += count;

                    // Break after first successful reproduction
                    break;
                }
            });

            // Process Doctor Boid reproduction
            auto q_doctor = w.query<const Position, const Velocity, ReproductionCooldown, const DoctorBoid, const Alive>();
            q_doctor.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                              ReproductionCooldown& cooldown, const DoctorBoid&, const Alive&) {
                // Skip if on cooldown
                if (cooldown.cooldown > 0.0f) return;

                bool is_infected = e.has<Infected>();

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_doctor;
                if (is_infected) {
                    effective_r_interact *= config.debuff_r_interact_doctor_infected;
                }

                // Query neighbors within effective interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only reproduce with same swarm type
                    if (!ne.has<DoctorBoid>()) continue;

                    // Check opposite sex requirement
                    bool parent_is_male = e.has<Male>();
                    bool neighbor_is_male = ne.has<Male>();
                    if (parent_is_male == neighbor_is_male) continue; // Same sex, skip

                    // Check neighbor's cooldown
                    if (!ne.has<ReproductionCooldown>()) continue;
                    const ReproductionCooldown& ncooldown = ne.get<ReproductionCooldown>();
                    if (ncooldown.cooldown > 0.0f) continue;

                    // Calculate effective reproduction probability (debuffed if infected)
                    float effective_p_offspring = config.p_offspring_doctor;
                    if (is_infected) {
                        effective_p_offspring *= config.debuff_p_offspring_doctor_infected;
                    }

                    // Try to reproduce
                    if (!try_reproduce(effective_p_offspring, rng)) continue;

                    // Calculate offspring count
                    int count = offspring_count(config.offspring_mean_doctor,
                                               config.offspring_stddev_doctor, rng);

                    if (count <= 0) continue;

                    // Get neighbor position for midpoint calculation
                    const Position& npos = ne.get<Position>();
                    float spawn_x = (pos.x + npos.x) / 2.0f;
                    float spawn_y = (pos.y + npos.y) / 2.0f;

                    // Check if both parents are infected
                    bool neighbor_infected = ne.has<Infected>();
                    bool child_infected = false;
                    if (is_infected && neighbor_infected) {
                        // Child gets contagion from ONE parent only
                        child_infected = try_infect(config.p_infect_doctor, rng);
                    }

                    // Spawn offspring
                    std::uniform_real_distribution<float> dist_angle(0.0f, TWO_PI);
                    std::uniform_real_distribution<float> dist_speed(0.0f, config.max_speed * 0.5f);
                    std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);

                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = dist_speed(rng);

                        auto child = w.entity()
                            .add<DoctorBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        // Assign sex
                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        // Infect child if applicable
                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    // Set cooldown for both parents
                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    // Update stats
                    stats.newborns_total += count;
                    stats.newborns_doctor += count;

                    // Break after first successful reproduction
                    break;
                }
            });

            // Process Antivax Boid reproduction
            auto q_antivax = w.query<const Position, const Velocity, ReproductionCooldown, const AntivaxBoid, const Alive>();
            q_antivax.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                              ReproductionCooldown& cooldown, const AntivaxBoid&, const Alive&) {
                // Skip if on cooldown
                if (cooldown.cooldown > 0.0f) return;

                bool is_infected = e.has<Infected>();

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_normal;
                if (is_infected) {
                    effective_r_interact *= config.debuff_r_interact_normal_infected;
                }

                // Query neighbors within effective interaction radius
                auto neighbors = grid.query_neighbors(pos.x, pos.y, effective_r_interact);

                for (const auto& [nid, dist] : neighbors) {
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    // Only reproduce with same swarm type
                    if (!ne.has<AntivaxBoid>()) continue;

                    // Check opposite sex requirement
                    bool parent_is_male = e.has<Male>();
                    bool neighbor_is_male = ne.has<Male>();
                    if (parent_is_male == neighbor_is_male) continue; // Same sex, skip

                    // Check neighbor's cooldown
                    if (!ne.has<ReproductionCooldown>()) continue;
                    const ReproductionCooldown& ncooldown = ne.get<ReproductionCooldown>();
                    if (ncooldown.cooldown > 0.0f) continue;

                    // Calculate effective reproduction probability (debuffed if infected)
                    float effective_p_offspring = config.p_offspring_normal;
                    if (is_infected) {
                        effective_p_offspring *= config.debuff_p_offspring_normal_infected;
                    }

                    // Try to reproduce
                    if (!try_reproduce(effective_p_offspring, rng)) continue;

                    // Calculate offspring count
                    int count = offspring_count(config.offspring_mean_normal,
                                               config.offspring_stddev_normal, rng);

                    if (count <= 0) continue;

                    // Get neighbor position for midpoint calculation
                    const Position& npos = ne.get<Position>();
                    float spawn_x = (pos.x + npos.x) / 2.0f;
                    float spawn_y = (pos.y + npos.y) / 2.0f;

                    // Check if both parents are infected
                    bool neighbor_infected = ne.has<Infected>();
                    bool child_infected = false;
                    if (is_infected && neighbor_infected) {
                        // Child gets contagion from ONE parent only
                        child_infected = try_infect(config.p_infect_normal, rng);
                    }

                    // Spawn offspring — inherit AntivaxBoid tag from parents
                    std::uniform_real_distribution<float> dist_angle(0.0f, TWO_PI);
                    std::uniform_real_distribution<float> dist_speed(0.0f, config.max_speed * 0.5f);
                    std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);

                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = dist_speed(rng);

                        auto child = w.entity()
                            .add<AntivaxBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        // Assign sex
                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        // Infect child if applicable
                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    // Set cooldown for both parents
                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    // Update stats
                    stats.newborns_total += count;
                    stats.newborns_antivax += count;

                    // Break after first successful reproduction
                    break;
                }
            });

            w.defer_end();
        });
}

void register_death_system(flecs::world& world) {
    world.system("DeathSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            SimStats& stats = w.get_mut<SimStats>();

            w.defer_begin();

            // Check for death by infection
            auto q = w.query<const InfectionState, const Infected, const Alive>();
            q.each([&](flecs::entity e, const InfectionState& infection, const Infected&, const Alive&) {
                if (should_die(infection.time_infected, config.t_death)) {
                    // Remove Alive tag (entity remains for stats)
                    e.remove<Alive>();

                    // Update stats
                    stats.dead_total++;
                    if (e.has<NormalBoid>()) {
                        stats.dead_normal++;
                    } else if (e.has<DoctorBoid>()) {
                        stats.dead_doctor++;
                    } else if (e.has<AntivaxBoid>()) {
                        stats.dead_antivax++;
                    }
                }
            });

            w.defer_end();
        });
}

void register_doctor_promotion_system(flecs::world& world) {
    world.system("DoctorPromotionSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            std::mt19937& rng = sim_rng();

            w.defer_begin();

            // Check normal boids for promotion
            auto q = w.query<const Health, const NormalBoid, const Alive>();
            q.each([&](flecs::entity e, const Health& health, const NormalBoid&, const Alive&) {
                if (try_promote(health.age, config.t_adult, config.p_become_doctor, rng)) {
                    // Promote to doctor
                    e.remove<NormalBoid>();
                    e.add<DoctorBoid>();
                    // Note: Stats will be automatically updated by UpdateStatsSystem
                }
            });

            w.defer_end();
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

            // Populate config and sim_state pointers for renderer
            rs.config = &w.get_mut<SimConfig>();
            rs.sim_state = &w.get_mut<SimulationState>();

            // Build render data for all alive boids
            auto q = w.query<const Position, const Velocity, const Heading, const Alive>();
            q.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                        const Heading& heading, const Alive&) {
                BoidRenderData brd;
                brd.x = pos.x;
                brd.y = pos.y;
                brd.angle = heading.angle;
                // Determine swarm type
                if (e.has<DoctorBoid>()) {
                    brd.swarm_type = 1;
                } else if (e.has<AntivaxBoid>()) {
                    brd.swarm_type = 2;
                } else {
                    brd.swarm_type = 0;
                }

                // Color: red if infected, else swarm-specific
                if (e.has<Infected>()) {
                    brd.color = RenderConfig::COLOR_INFECTED;
                } else if (brd.swarm_type == 2) {
                    brd.color = RenderConfig::COLOR_ANTIVAX;
                } else if (brd.swarm_type == 1) {
                    brd.color = RenderConfig::COLOR_DOCTOR;
                } else {
                    brd.color = RenderConfig::COLOR_NORMAL;
                }

                brd.radius = (brd.swarm_type == 1) ? config.r_interact_doctor : config.r_interact_normal;
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
    register_antivax_steering_system(world);
    register_movement_system(world);

    // PostUpdate
    register_aging_system(world);
    register_collision_system(world);
    register_infection_system(world);
    register_cure_system(world);
    register_reproduction_system(world);
    register_death_system(world);
    register_doctor_promotion_system(world);

    // OnStore
    register_render_sync_system(world);
}
