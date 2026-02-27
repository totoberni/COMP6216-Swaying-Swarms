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

            // --- Swarm 2: AntivaxBoid ---
            positions.clear();
            velocities.clear();
            auto q2 = w.query<const Position, const Velocity, const AntivaxBoid>();
            q2.each([&](const Position& pos, const Velocity& vel, const AntivaxBoid&) {
                positions.push_back({pos.x, pos.y});
                velocities.push_back({vel.vx, vel.vy});
            });
            compute_swarm_metrics(stats.swarm[2], positions, velocities);
        });
}
