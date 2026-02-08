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

            // Note: dead_total, dead_normal, dead_doctor, newborns_* are cumulative
            // and updated by the death and reproduction systems
        });
}
