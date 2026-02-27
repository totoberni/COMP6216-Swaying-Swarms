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

            // --------- Average Cohesion & Separation -----------
            int live_normal_count = 0;
            auto q_positions = w.query<const Position, const NormalBoid>();
            q_positions.each([&](Position pos, const NormalBoid&) {
                stats.pos_avg = Vector2Add(stats.pos_avg, {pos.x, pos.y});
                live_normal_count++;
            });

            if (live_normal_count > 0) {
                stats.pos_avg = Vector2Scale(stats.pos_avg, 1.0f / live_normal_count);

                float sum_sq_dist = 0.0f;
                q_positions.each([&](Position pos, const NormalBoid&) {
                    Vector2 diff = Vector2Subtract({pos.x, pos.y}, stats.pos_avg);
                    stats.average_cohesion += Vector2Length(diff);
                    sum_sq_dist += Vector2LengthSqr(diff);
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
            }

            // --------- Average Alignment----------
            auto q_velocities = w.query<const Velocity, const NormalBoid>();
            q_velocities.each([&](Velocity vel, const NormalBoid&) {
                stats.vel_avg = Vector2Add(stats.vel_avg, {vel.vx, vel.vy});
            });

            if (live_normal_count > 0) {
                stats.vel_avg = Vector2Scale(stats.vel_avg, 1.0f / live_normal_count);

                q_velocities.each([&](Velocity vel, const NormalBoid&) {
                    // find the angle between boid and true alignment
                    float theta = Vector2Angle(stats.vel_avg, Vector2{vel.vx, vel.vy});
                    stats.average_alignment_angle += theta;
                });

                stats.ali_history[stats.ali_history_index] = stats.average_alignment_angle / live_normal_count;
                stats.ali_history_index = (stats.ali_history_index + 1) % SimStats::HISTORY_SIZE;
                if (stats.ali_history_count < SimStats::HISTORY_SIZE) {
                    stats.ali_history_count++;
                }
            }
        });
}
