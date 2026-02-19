#include "systems.h"
#include "components.h"
#include "sim/aging.h"
#include "sim/death.h"
#include "sim/promotion.h"
#include "sim/rng.h"
#include <flecs.h>
#include <vector>

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
// PostUpdate Phase: Death
// ============================================================

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

// ============================================================
// PostUpdate Phase: Doctor Promotion
// ============================================================

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
                }
            });

            w.defer_end();
        });
}

// ============================================================
// OnStore Phase: Dead Entity Cleanup (must run after render sync)
// ============================================================

void register_cleanup_system(flecs::world& world) {
    world.system("CleanupSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();

            // Collect entities to destroy (have Position but not Alive)
            // Cannot destroy during query iteration, so collect first
            std::vector<flecs::entity> to_destroy;
            auto q = w.query<const Position>();
            q.each([&](flecs::entity e, const Position&) {
                if (!e.has<Alive>()) {
                    to_destroy.push_back(e);
                }
            });

            for (auto e : to_destroy) {
                e.destruct();
            }
        });
}
