#include "stats.h"
#include "components.h"
#include <flecs.h>
#include <raylib.h>

void register_stats_system(flecs::world& world) {
    world.system("UpdateStatsSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SimStats& stats = w.get_mut<SimStats>();

            // Reset per-frame counters (deaths and newborns are cumulative)
            stats.normal_alive = 0;
            stats.doctor_alive = 0;
            stats.pos_avg = Vector2Zero();

            // Count alive normal boids
            auto q_normal = w.query<const NormalBoid>();
            q_normal.each([&stats](const NormalBoid&) {
                stats.normal_alive++;
            });

            // Count alive doctor boids
            auto q_doctor = w.query<const DoctorBoid>();
            q_doctor.each([&stats](const DoctorBoid&) {
                stats.doctor_alive++;
            });

            // Note: dead_total, dead_normal, dead_doctor, newborns_* are cumulative
            // and updated by the death and reproduction systems

            // Count total infected across all swarms
            int infected_count = 0;
            auto q_infected = w.query<const Infected>();
            q_infected.each([&infected_count](const Infected&) {
                infected_count++;
            });

            // Record population history for graph
            stats.history[stats.history_index].normal_alive = stats.normal_alive;
            stats.history[stats.history_index].doctor_alive = stats.doctor_alive;
            stats.history[stats.history_index].infected_count = infected_count;
            stats.history_index = (stats.history_index + 1) % SimStats::HISTORY_SIZE;
            if (stats.history_count < SimStats::HISTORY_SIZE) {
                stats.history_count++;
            }

            // Average Cohesion
            auto q_positions = w.query<const Position, const NormalBoid>();
            q_positions.each([&stats](Position pos, const NormalBoid&) {
                stats.pos_avg.x = stats.pos_avg.x + pos.x;
                stats.pos_avg.y = stats.pos_avg.y + pos.y;
            });

            stats.pos_avg.x = stats.pos_avg.x / static_cast<float>(stats.normal_alive);
            stats.pos_avg.y = stats.pos_avg.y / static_cast<float>(stats.normal_alive);
        });
}