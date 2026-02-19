#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "sim/infection.h"
#include "sim/reproduction.h"
#include "sim/rng.h"
#include <flecs.h>
#include <cmath>
#include <vector>
#include <utility>

namespace {
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 2.0f * PI;
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
            std::uniform_real_distribution<float> dist_angle(0.0f, TWO_PI);
            std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);
            float dt = it.delta_time();

            // First, update cooldowns for all alive boids
            auto q_cooldown = w.query<ReproductionCooldown, const Alive>();
            q_cooldown.each([dt](ReproductionCooldown& cooldown, const Alive&) {
                if (cooldown.cooldown > 0.0f) {
                    cooldown.cooldown -= dt;
                }
            });

            w.defer_begin();

            std::vector<SpatialGrid::QueryResult> neighbors;

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
                grid.query_neighbors(pos.x, pos.y, effective_r_interact, neighbors);

                for (const auto& qr : neighbors) {
                    uint64_t nid = qr.entry->entity_id;
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
                        child_infected = try_infect(config.p_infect_normal, rng);
                    }

                    // Spawn offspring
                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = config.max_speed;

                        auto child = w.entity()
                            .add<NormalBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    stats.newborns_total += count;
                    stats.newborns_normal += count;

                    break;
                }
            });

            // Process Doctor Boid reproduction
            auto q_doctor = w.query<const Position, const Velocity, ReproductionCooldown, const DoctorBoid, const Alive>();
            q_doctor.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                              ReproductionCooldown& cooldown, const DoctorBoid&, const Alive&) {
                if (cooldown.cooldown > 0.0f) return;

                bool is_infected = e.has<Infected>();

                float effective_r_interact = config.r_interact_doctor;
                if (is_infected) {
                    effective_r_interact *= config.debuff_r_interact_doctor_infected;
                }

                grid.query_neighbors(pos.x, pos.y, effective_r_interact, neighbors);

                for (const auto& qr : neighbors) {
                    uint64_t nid = qr.entry->entity_id;
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    if (!ne.has<DoctorBoid>()) continue;

                    bool parent_is_male = e.has<Male>();
                    bool neighbor_is_male = ne.has<Male>();
                    if (parent_is_male == neighbor_is_male) continue;

                    if (!ne.has<ReproductionCooldown>()) continue;
                    const ReproductionCooldown& ncooldown = ne.get<ReproductionCooldown>();
                    if (ncooldown.cooldown > 0.0f) continue;

                    float effective_p_offspring = config.p_offspring_doctor;
                    if (is_infected) {
                        effective_p_offspring *= config.debuff_p_offspring_doctor_infected;
                    }

                    if (!try_reproduce(effective_p_offspring, rng)) continue;

                    int count = offspring_count(config.offspring_mean_doctor,
                                               config.offspring_stddev_doctor, rng);

                    if (count <= 0) continue;

                    const Position& npos = ne.get<Position>();
                    float spawn_x = (pos.x + npos.x) / 2.0f;
                    float spawn_y = (pos.y + npos.y) / 2.0f;

                    bool neighbor_infected = ne.has<Infected>();
                    bool child_infected = false;
                    if (is_infected && neighbor_infected) {
                        child_infected = try_infect(config.p_infect_doctor, rng);
                    }

                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = config.max_speed;

                        auto child = w.entity()
                            .add<DoctorBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    stats.newborns_total += count;
                    stats.newborns_doctor += count;

                    break;
                }
            });

            // Process Antivax Boid reproduction
            auto q_antivax = w.query<const Position, const Velocity, ReproductionCooldown, const AntivaxBoid, const Alive>();
            q_antivax.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                              ReproductionCooldown& cooldown, const AntivaxBoid&, const Alive&) {
                if (cooldown.cooldown > 0.0f) return;

                bool is_infected = e.has<Infected>();

                float effective_r_interact = config.r_interact_normal;
                if (is_infected) {
                    effective_r_interact *= config.debuff_r_interact_normal_infected;
                }

                grid.query_neighbors(pos.x, pos.y, effective_r_interact, neighbors);

                for (const auto& qr : neighbors) {
                    uint64_t nid = qr.entry->entity_id;
                    if (nid == e.id()) continue;

                    flecs::entity ne = w.entity(nid);
                    if (!ne.is_alive() || !ne.has<Alive>()) continue;

                    if (!ne.has<AntivaxBoid>()) continue;

                    bool parent_is_male = e.has<Male>();
                    bool neighbor_is_male = ne.has<Male>();
                    if (parent_is_male == neighbor_is_male) continue;

                    if (!ne.has<ReproductionCooldown>()) continue;
                    const ReproductionCooldown& ncooldown = ne.get<ReproductionCooldown>();
                    if (ncooldown.cooldown > 0.0f) continue;

                    float effective_p_offspring = config.p_offspring_normal;
                    if (is_infected) {
                        effective_p_offspring *= config.debuff_p_offspring_normal_infected;
                    }

                    if (!try_reproduce(effective_p_offspring, rng)) continue;

                    int count = offspring_count(config.offspring_mean_normal,
                                               config.offspring_stddev_normal, rng);

                    if (count <= 0) continue;

                    const Position& npos = ne.get<Position>();
                    float spawn_x = (pos.x + npos.x) / 2.0f;
                    float spawn_y = (pos.y + npos.y) / 2.0f;

                    bool neighbor_infected = ne.has<Infected>();
                    bool child_infected = false;
                    if (is_infected && neighbor_infected) {
                        child_infected = try_infect(config.p_infect_normal, rng);
                    }

                    // Spawn offspring — inherit AntivaxBoid tag from parents
                    for (int i = 0; i < count; ++i) {
                        float angle = dist_angle(rng);
                        float speed = config.max_speed;

                        auto child = w.entity()
                            .add<AntivaxBoid>()
                            .add<Alive>()
                            .set(Position{spawn_x, spawn_y})
                            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
                            .set(Heading{angle})
                            .set(Health{0.0f, 60.0f})
                            .set(ReproductionCooldown{config.reproduction_cooldown});

                        if (dist_sex(rng) < 0.5f) {
                            child.add<Male>();
                        } else {
                            child.add<Female>();
                        }

                        if (child_infected) {
                            child.add<Infected>();
                            child.set(InfectionState{0.0f, config.t_death});
                        }
                    }

                    cooldown.cooldown = config.reproduction_cooldown;
                    ReproductionCooldown& ncool = ne.get_mut<ReproductionCooldown>();
                    ncool.cooldown = config.reproduction_cooldown;

                    stats.newborns_total += count;
                    stats.newborns_antivax += count;

                    break;
                }
            });

            w.defer_end();
        });
}
