#include "systems_doctor.h"
#include "components.h"
#include "spatial_grid.h"
#include "toroidal.h"
#include <flecs.h>
#include <raymath.h>
#include <cmath>
#include <vector>

void register_doctor_systems(flecs::world& world) {

    // D2 — Seek Nearest Infected (PostUpdate)
    // Only active when config.doctor_behavior == SeekNearest.
    // Queries spatial grid for closest infected boid within doctor_seek_radius,
    // then applies a Reynolds-style seek force toward it.
    world.system("DoctorSeekNearestSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            if (config.doctor_behavior != DoctorBehavior::SeekNearest) return;

            const SpatialGrid& grid = w.get<SpatialGrid>();
            float dt = it.delta_time();
            float seek_r_sq = config.doctor_seek_radius * config.doctor_seek_radius;

            std::vector<SpatialGrid::QueryResult> neighbors;
            neighbors.reserve(64);

            auto q = w.query<const Position, Velocity, const DoctorBoid>();
            q.each([&](const Position& pos, Velocity& vel, const DoctorBoid&) {
                grid.query_neighbors(pos.x, pos.y, config.doctor_seek_radius, neighbors);

                // Find nearest infected entry
                float best_dist_sq = seek_r_sq + 1.0f;
                const SpatialGrid::Entry* nearest = nullptr;

                for (const auto& qr : neighbors) {
                    if (!qr.entry->infected) continue;
                    if (qr.dist_sq < best_dist_sq) {
                        best_dist_sq = qr.dist_sq;
                        nearest = qr.entry;
                    }
                }

                if (!nearest) return;

                // Reynolds seek with toroidal displacement
                float tdx, tdy;
                displacement(pos.x, pos.y, nearest->x, nearest->y,
                             !config.wall_bounce, config.world_width, config.world_height, tdx, tdy);
                Vector2 toward = {tdx, tdy};
                float mag = Vector2Length(toward);
                if (mag < 0.001f) return;

                Vector2 desired = Vector2Scale(toward, config.doctor.max_speed / mag);
                Vector2 current = {vel.vx, vel.vy};
                Vector2 seek = Vector2Subtract(desired, current);
                seek = Vector2Scale(seek, config.doctor_seek_weight);

                vel.vx += seek.x * dt;
                vel.vy += seek.y * dt;

                // Re-clamp to per-swarm max_speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);
                if (speed > config.doctor.max_speed) {
                    v = Vector2Scale(v, config.doctor.max_speed / speed);
                    vel.vx = v.x;
                    vel.vy = v.y;
                }
            });
        });

    // D3 — Seek Sick Centroid (PostUpdate)
    // Only active when config.doctor_behavior == SeekCentroid.
    // Reads sick centroid from SimStats (computed previous frame by OnStore stats),
    // applies Reynolds-style seek force toward it.
    world.system("DoctorSeekCentroidSystem")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            if (config.doctor_behavior != DoctorBehavior::SeekCentroid) return;

            const SimStats& stats = w.get<SimStats>();
            if (stats.total_infected == 0) return; // centroid undefined

            float dt = it.delta_time();
            float cx = stats.sick_centroid_x;
            float cy = stats.sick_centroid_y;

            auto q = w.query<const Position, Velocity, const DoctorBoid>();
            q.each([&](const Position& pos, Velocity& vel, const DoctorBoid&) {
                float tdx, tdy;
                displacement(pos.x, pos.y, cx, cy,
                             !config.wall_bounce, config.world_width, config.world_height, tdx, tdy);
                Vector2 toward = {tdx, tdy};
                float mag = Vector2Length(toward);
                if (mag < 0.001f) return;

                Vector2 desired = Vector2Scale(toward, config.doctor.max_speed / mag);
                Vector2 current = {vel.vx, vel.vy};
                Vector2 seek = Vector2Subtract(desired, current);
                seek = Vector2Scale(seek, config.doctor_seek_weight);

                vel.vx += seek.x * dt;
                vel.vy += seek.y * dt;

                // Re-clamp to per-swarm max_speed
                Vector2 v = {vel.vx, vel.vy};
                float speed = Vector2Length(v);
                if (speed > config.doctor.max_speed) {
                    v = Vector2Scale(v, config.doctor.max_speed / speed);
                    vel.vx = v.x;
                    vel.vy = v.y;
                }
            });
        });
}
