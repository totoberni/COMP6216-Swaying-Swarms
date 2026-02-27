#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "sim/infection.h"
#include "sim/cure.h"
#include "sim/rng.h"
#include <flecs.h>
#include <vector>
#include <algorithm>
#include <random>

// ============================================================
// PostUpdate Phase: Collisions and Behavior
// ============================================================

void register_collision_system(flecs::world& world) {
    // Collision system stub reserved for future extensions.
    (void)world;
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

            std::vector<SpatialGrid::QueryResult> neighbors;

            // Unified infection pass: iterate all infected alive boids
            auto q_infected = w.query<const Position, const Infected>();
            q_infected.each([&](flecs::entity e, const Position& pos, const Infected&) {
                // Determine spreader's swarm type and effective radius/probability
                bool is_doctor = e.has<DoctorBoid>();
                float effective_r_interact;
                float p_infect;

                if (is_doctor) {
                    effective_r_interact = config.r_interact_doctor * config.debuff_r_interact_doctor_infected;
                    p_infect = config.p_infect_doctor;
                } else {
                    // Normal and Antivax use the same infection params
                    effective_r_interact = config.r_interact_normal * config.debuff_r_interact_normal_infected;
                    p_infect = config.p_infect_normal;
                }

                // Query neighbors within effective interaction radius
                grid.query_neighbors(pos.x, pos.y, effective_r_interact, neighbors);

                for (const auto& qr : neighbors) {
                    const auto* ne_entry = qr.entry;
                    if (ne_entry->entity_id == e.id()) continue;

                    // Use enriched entry: skip already infected
                    if (ne_entry->infected) continue;

                    // Free cross-swarm infection: any infected boid can infect any susceptible boid
                    // Apply immunity reduction to infection probability
                    float effective_p = p_infect;
                    flecs::entity ne = w.entity(ne_entry->entity_id);
                    bool has_immunity = ne.has<ImmunityState>();
                    if (has_immunity) {
                        const ImmunityState& imm = ne.get<ImmunityState>();
                        effective_p *= (1.0f - imm.immunity_level);
                    }

                    if (try_infect(effective_p, rng)) {
                        ne.add<Infected>();
                        ne.set(InfectionState{0.0f, config.t_death});
                        if (has_immunity) ne.remove<ImmunityState>();
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
            auto q_doctor = w.query<const Position, const DoctorBoid>();
            std::vector<SpatialGrid::QueryResult> neighbors;
            q_doctor.each([&](flecs::entity e, const Position& pos, const DoctorBoid&) {
                // Check if doctor is infected for debuff calculation
                bool doctor_infected = e.has<Infected>();

                // Calculate effective interaction radius (debuffed if infected)
                float effective_r_interact = config.r_interact_doctor;
                if (doctor_infected) {
                    effective_r_interact *= config.debuff_r_interact_doctor_infected;
                }

                // Query neighbors within effective doctor interaction radius
                grid.query_neighbors(pos.x, pos.y, effective_r_interact, neighbors);

                for (const auto& qr : neighbors) {
                    const auto* ne_entry = qr.entry;
                    // Contract: doctors cannot cure themselves
                    if (ne_entry->entity_id == e.id()) continue;

                    // Use enriched entry: skip non-alive, skip non-infected
                    if (!(ne_entry->infected)) continue;

                    // Calculate effective cure probability (debuffed if doctor is infected)
                    float effective_p_cure = config.p_cure;
                    if (doctor_infected) {
                        effective_p_cure *= config.debuff_p_cure_infected;
                    }

                    // Try to cure (doctors cure ANY infected boid, including other doctors)
                    // Cure grants partial immunity to prevent immediate re-infection
                    if (try_cure(effective_p_cure, rng)) {
                        flecs::entity ne = w.entity(ne_entry->entity_id);
                        ne.remove<Infected>();
                        ne.remove<InfectionState>();
                        ne.set(ImmunityState{config.cure_immunity_level, 0.0f});
                    }
                }
            });

            w.defer_end();
        });
}

void register_death_recovery_system(flecs::world& world) {
    world.system<InfectionState>("DeathRecoverySystem")
        .with<Infected>()
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, InfectionState& inf) {
            flecs::world w = e.world();
            const SimConfig& config = w.get<SimConfig>();
            float dt = w.delta_time();

            inf.time_infected += dt;

            if (inf.time_infected >= inf.time_to_death) {
                std::mt19937& rng = sim_rng();
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                if (dist(rng) < config.p_death_infected) {
                    // Death
                    e.destruct();
                } else {
                    // Survived — gain full immunity
                    e.remove<Infected>();
                    e.remove<InfectionState>();
                    e.set(ImmunityState{1.0f, 0.0f});
                }
            }
        });
}

void register_immunity_decay_system(flecs::world& world) {
    world.system<ImmunityState>("ImmunityDecaySystem")
        .without<Infected>()
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, ImmunityState& imm) {
            flecs::world w = e.world();
            const SimConfig& config = w.get<SimConfig>();
            float dt = w.delta_time();

            imm.time_since_recovery += dt;
            imm.immunity_level = std::max(0.0f, 1.0f - imm.time_since_recovery / config.t_immunity);

            if (imm.immunity_level <= 0.0f) {
                e.remove<ImmunityState>();
            }
        });
}
