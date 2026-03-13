#include "stats.h"
#include "components.h"
#include <flecs.h>
#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <vector>

// Helper: compute per-swarm metrics from a list of (pos, vel) pairs
static void compute_swarm_metrics(SwarmMetrics& m,
                                  const std::vector<Vector2>& positions,
                                  const std::vector<Vector2>& velocities) {
    int count = static_cast<int>(positions.size());
    m.alive = count;
    m.pos_avg = Vector2Zero();
    m.vel_avg = Vector2Zero();
    m.average_cohesion = 0.0f;
    m.average_alignment_angle = 0.0f;
    m.average_separation = 0.0f;

    if (count == 0) return;

    // First pass: accumulate sums
    for (int i = 0; i < count; ++i) {
        m.pos_avg = Vector2Add(m.pos_avg, positions[i]);
        m.vel_avg = Vector2Add(m.vel_avg, velocities[i]);
    }
    m.pos_avg = Vector2Scale(m.pos_avg, 1.0f / count);
    m.vel_avg = Vector2Scale(m.vel_avg, 1.0f / count);

    // Second pass: compute deviations
    float sum_sq_dist = 0.0f;
    for (int i = 0; i < count; ++i) {
        Vector2 pos_diff = Vector2Subtract(positions[i], m.pos_avg);
        m.average_cohesion += Vector2Length(pos_diff);
        sum_sq_dist += Vector2LengthSqr(pos_diff);

        float theta = Vector2Angle(m.vel_avg, velocities[i]);
        m.average_alignment_angle += theta;
    }

    // Write to history buffers
    m.coh_history[m.coh_history_index] = m.average_cohesion / count;
    m.coh_history_index = (m.coh_history_index + 1) % SwarmMetrics::HISTORY_SIZE;
    if (m.coh_history_count < SwarmMetrics::HISTORY_SIZE) m.coh_history_count++;

    // RMS separation via Huygens-Steiner theorem
    if (count > 1) {
        m.average_separation = std::sqrt(2.0f * sum_sq_dist / (count - 1));
    }
    m.sep_history[m.sep_history_index] = m.average_separation;
    m.sep_history_index = (m.sep_history_index + 1) % SwarmMetrics::HISTORY_SIZE;
    if (m.sep_history_count < SwarmMetrics::HISTORY_SIZE) m.sep_history_count++;

    m.ali_history[m.ali_history_index] = m.average_alignment_angle / count;
    m.ali_history_index = (m.ali_history_index + 1) % SwarmMetrics::HISTORY_SIZE;
    if (m.ali_history_count < SwarmMetrics::HISTORY_SIZE) m.ali_history_count++;
}

void register_stats_system(flecs::world& world) {
    world.system("UpdateStatsSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SimStats& stats = w.get_mut<SimStats>();

            // Temporary storage for per-swarm pos/vel
            std::vector<Vector2> positions;
            std::vector<Vector2> velocities;
            positions.reserve(256);
            velocities.reserve(256);

            // --- Swarm 0: NormalBoid ---
            positions.clear();
            velocities.clear();
            auto q0 = w.query<const Position, const Velocity, const NormalBoid>();
            q0.each([&](const Position& pos, const Velocity& vel, const NormalBoid&) {
                positions.push_back({pos.x, pos.y});
                velocities.push_back({vel.vx, vel.vy});
            });
            compute_swarm_metrics(stats.swarm[0], positions, velocities);

            // --- Swarm 1: DoctorBoid ---
            positions.clear();
            velocities.clear();
            auto q1 = w.query<const Position, const Velocity, const DoctorBoid>();
            q1.each([&](const Position& pos, const Velocity& vel, const DoctorBoid&) {
                positions.push_back({pos.x, pos.y});
                velocities.push_back({vel.vx, vel.vy});
            });
            compute_swarm_metrics(stats.swarm[1], positions, velocities);

            // --- Sickness metrics ---
            int prev_infected = stats.total_infected;
            int infected_count = 0;
            float sick_sum_x = 0.0f, sick_sum_y = 0.0f;
            float sick_sum_angle = 0.0f;

            auto q_sick = w.query<const Position, const Heading, const Infected>();
            q_sick.each([&](const Position& pos, const Heading& h, const Infected&) {
                infected_count++;
                sick_sum_x += pos.x;
                sick_sum_y += pos.y;
                sick_sum_angle += h.angle;
            });

            int recovered_count = 0;
            auto q_recovered = w.query<const ImmunityState>();
            q_recovered.each([&](flecs::entity e, const ImmunityState& imm) {
                if (imm.immunity_level > 0.0f && !e.has<Infected>()) {
                    recovered_count++;
                }
            });

            stats.total_infected = infected_count;
            stats.total_recovered = recovered_count;

            int total_pop = stats.swarm[0].alive + stats.swarm[1].alive;
            stats.pct_infected = (total_pop > 0)
                ? static_cast<float>(infected_count) / static_cast<float>(total_pop)
                : 0.0f;

            float dt = w.delta_time();
            if (dt > 0.0f) {
                float raw_rate = static_cast<float>(infected_count - prev_infected) / dt;
                stats.infection_growth_rate = 0.9f * stats.infection_growth_rate + 0.1f * raw_rate;
            }

            if (infected_count > 0) {
                stats.sick_centroid_x = sick_sum_x / infected_count;
                stats.sick_centroid_y = sick_sum_y / infected_count;
                stats.sick_avg_alignment = sick_sum_angle / infected_count;
            } else {
                stats.sick_centroid_x = 0.0f;
                stats.sick_centroid_y = 0.0f;
                stats.sick_avg_alignment = 0.0f;
            }

            // Write to sickness history buffers (reuse history_index for sync)
            int hi = stats.history_index;
            stats.infected_count_history[hi] = static_cast<float>(infected_count);
            stats.pct_infected_history[hi] = stats.pct_infected;
            stats.growth_rate_history[hi] = stats.infection_growth_rate;
            stats.recovered_count_history[hi] = static_cast<float>(recovered_count);
        });
}
