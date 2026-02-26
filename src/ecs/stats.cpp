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
            const SimConfig& config = w.get<SimConfig>();

            // Reset per-frame counters (deaths and newborns are cumulative)
            // stats.normal_alive = 0;
            // stats.doctor_alive = 0;
            stats.pos_avg = Vector2Zero();
            stats.average_cohesion = 0.;
            stats.vel_avg = Vector2Zero();
            stats.average_alignment_angle = 0.;

            /* Count alive normal boids
            auto q_normal = w.query<const NormalBoid>();
            q_normal.each([&stats](const NormalBoid&) {
                stats.normal_alive++;
            });
            */

            /* Count alive doctor boids
            auto q_doctor = w.query<const DoctorBoid>();
            q_doctor.each([&stats](const DoctorBoid&) {
                stats.doctor_alive++;
            });*/

            /* Count total infected across all swarms
            int infected_count = 0;
            auto q_infected = w.query<const Infected>();
            q_infected.each([&infected_count](const Infected&) {
                infected_count++;
            });*/

            // --------- Average Cohesion -----------
            auto q_positions = w.query<const Position, const NormalBoid>();
            q_positions.each([&stats](Position pos, const NormalBoid&) {
                stats.pos_avg.x = stats.pos_avg.x + pos.x;
                stats.pos_avg.y = stats.pos_avg.y + pos.y;
            });

            stats.pos_avg.x = stats.pos_avg.x / static_cast<float>(config.initial_normal_count);
            stats.pos_avg.y = stats.pos_avg.y / static_cast<float>(config.initial_normal_count);

            q_positions.each([&stats](Position pos, const NormalBoid&) {
                float dist_x = pos.x - stats.pos_avg.x;
                float dist_y = pos.y - stats.pos_avg.y;
                
                stats.average_cohesion = stats.average_cohesion + sqrtf((dist_x * dist_x) + (dist_y * dist_y));
            });

            stats.coh_history[stats.coh_history_index] = stats.average_cohesion / static_cast<float>(config.initial_normal_count);
            stats.coh_history_index = (stats.coh_history_index + 1) % SimStats::HISTORY_SIZE;
            if (stats.coh_history_count < SimStats::HISTORY_SIZE) {
                stats.coh_history_count++;
            }

            // --------- Average Alignment----------
            auto q_velocities = w.query<const Velocity, const NormalBoid>();
            q_velocities.each([&stats](Velocity vel, const NormalBoid&) {
                stats.vel_avg.x = stats.vel_avg.x + vel.vx;
                stats.vel_avg.y = stats.vel_avg.y + vel.vy;
            });

            stats.vel_avg.x = stats.vel_avg.x / static_cast<float>(config.initial_normal_count);
            stats.vel_avg.y = stats.vel_avg.y / static_cast<float>(config.initial_normal_count);

            q_velocities.each([&stats](Velocity vel, const NormalBoid&) {
                // find the angle between boid and true alignment

                float theta = Vector2Angle(stats.vel_avg, Vector2{vel.vx, vel.vy});
                
                stats.average_alignment_angle = stats.average_alignment_angle + theta;
            });

            stats.ali_history[stats.ali_history_index] = stats.average_alignment_angle / static_cast<float>(config.initial_normal_count);
            stats.ali_history_index = (stats.ali_history_index + 1) % SimStats::HISTORY_SIZE;
            if (stats.ali_history_count < SimStats::HISTORY_SIZE) {
                stats.ali_history_count++;
            }

            // Record population history for graph
            // stats.history[stats.history_index].normal_alive = stats.normal_alive;
            // stats.history[stats.history_index].doctor_alive = stats.doctor_alive;
            // stats.history[stats.history_index].infected_count = infected_count;
            // stats.history_index = (stats.history_index + 1) % SimStats::HISTORY_SIZE;
            // if (stats.history_count < SimStats::HISTORY_SIZE) {
                // stats.history_count++;
            // }
        });
}