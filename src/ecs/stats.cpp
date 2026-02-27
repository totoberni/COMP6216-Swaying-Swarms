#include "stats.h"
#include "components.h"
#include <flecs.h>
#include <raylib.h>
#include <raymath.h>
#include <cmath>

void register_stats_system(flecs::world& world) {
    world.system("UpdateStatsSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SimStats& stats = w.get_mut<SimStats>();
            const SimConfig& config = w.get<SimConfig>();

            // Reset per-frame counters
            stats.pos_avg = Vector2Zero();
            stats.average_cohesion = 0.0f;
            stats.vel_avg = Vector2Zero();
            stats.average_alignment_angle = 0.0f;
            stats.average_separation = 0.0f;

            // --------- Single pass: accumulate position + velocity sums -----------
            int live_normal_count = 0;
            auto q = w.query<const Position, const Velocity, const NormalBoid>();
            q.each([&](Position pos, Velocity vel, const NormalBoid&) {
                stats.pos_avg = Vector2Add(stats.pos_avg, {pos.x, pos.y});
                stats.vel_avg = Vector2Add(stats.vel_avg, {vel.vx, vel.vy});
                live_normal_count++;
            });
            stats.normal_alive = live_normal_count;

            // Count doctor boids
            int live_doctor_count = 0;
            w.query<const DoctorBoid>().each([&](const DoctorBoid&) {
                live_doctor_count++;
            });
            stats.doctor_alive = live_doctor_count;

            if (live_normal_count > 0) {
                stats.pos_avg = Vector2Scale(stats.pos_avg, 1.0f / live_normal_count);
                stats.vel_avg = Vector2Scale(stats.vel_avg, 1.0f / live_normal_count);

                // Second pass: compute deviations from averages
                float sum_sq_dist = 0.0f;
                q.each([&](Position pos, Velocity vel, const NormalBoid&) {
                    Vector2 pos_diff = Vector2Subtract({pos.x, pos.y}, stats.pos_avg);
                    stats.average_cohesion += Vector2Length(pos_diff);
                    sum_sq_dist += Vector2LengthSqr(pos_diff);

                    float theta = Vector2Angle(stats.vel_avg, Vector2{vel.vx, vel.vy});
                    stats.average_alignment_angle += theta;
                });

                stats.coh_history[stats.coh_history_index] = stats.average_cohesion / live_normal_count;
                stats.coh_history_index = (stats.coh_history_index + 1) % SimStats::HISTORY_SIZE;
                if (stats.coh_history_count < SimStats::HISTORY_SIZE) {
                    stats.coh_history_count++;
                }

                // RMS separation via Huygens-Steiner theorem
                if (live_normal_count > 1) {
                    stats.average_separation = std::sqrt(2.0f * sum_sq_dist / (live_normal_count - 1));
                }

                stats.sep_history[stats.sep_history_index] = stats.average_separation;
                stats.sep_history_index = (stats.sep_history_index + 1) % SimStats::HISTORY_SIZE;
                if (stats.sep_history_count < SimStats::HISTORY_SIZE) {
                    stats.sep_history_count++;
                }

                stats.ali_history[stats.ali_history_index] = stats.average_alignment_angle / live_normal_count;
                stats.ali_history_index = (stats.ali_history_index + 1) % SimStats::HISTORY_SIZE;
                if (stats.ali_history_count < SimStats::HISTORY_SIZE) {
                    stats.ali_history_count++;
                }
            }
        });
}
