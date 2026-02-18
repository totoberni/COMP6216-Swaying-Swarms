#include "stats.h"
#include "components.h"
#include <flecs.h>

void register_stats_system(flecs::world& world) {
    world.system("UpdateStatsSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SimStats& stats = w.get_mut<SimStats>();

            // Reset per-frame counters (deaths and newborns are cumulative)
            stats.normal_alive = 0;
            stats.doctor_alive = 0;
            stats.antivax_alive = 0;

            // Count alive normal boids
            auto q_normal = w.query<const NormalBoid, const Alive>();
            q_normal.each([&stats](const NormalBoid&, const Alive&) {
                stats.normal_alive++;
            });

            // Count alive doctor boids
            auto q_doctor = w.query<const DoctorBoid, const Alive>();
            q_doctor.each([&stats](const DoctorBoid&, const Alive&) {
                stats.doctor_alive++;
            });

            // Count alive antivax boids
            auto q_antivax = w.query<const AntivaxBoid, const Alive>();
            q_antivax.each([&stats](const AntivaxBoid&, const Alive&) {
                stats.antivax_alive++;
            });

            // Note: dead_total, dead_normal, dead_doctor, newborns_* are cumulative
            // and updated by the death and reproduction systems

            // Record population history for graph
            stats.history[stats.history_index].normal_alive = stats.normal_alive;
            stats.history[stats.history_index].doctor_alive = stats.doctor_alive;
            stats.history[stats.history_index].antivax_alive = stats.antivax_alive;
            stats.history_index = (stats.history_index + 1) % SimStats::HISTORY_SIZE;
            if (stats.history_count < SimStats::HISTORY_SIZE) {
                stats.history_count++;
            }
        });
}
