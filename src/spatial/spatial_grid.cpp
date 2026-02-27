#include "spatial_grid.h"
#include <cmath>
#include <raymath.h>

namespace {
    // Small epsilon to prevent entities from landing exactly on upper boundary
    constexpr float BOUNDARY_EPSILON = 0.001f;
}

SpatialGrid::SpatialGrid(float world_w, float world_h, float cell_size)
    : world_w_(world_w)
    , world_h_(world_h)
    , cell_size_(cell_size)
    , cols_(static_cast<int>(std::ceil(world_w / cell_size)))
    , rows_(static_cast<int>(std::ceil(world_h / cell_size)))
{
    cells_.resize(cols_ * rows_);
}

void SpatialGrid::clear() {
    for (auto& cell : cells_) {
        cell.clear();
    }
}

void SpatialGrid::insert(uint64_t entity_id, float x, float y,
                          float vx, float vy, uint8_t swarm_type, bool infected) {
    // Clamp to grid bounds (epsilon prevents exact boundary hits causing out-of-bounds indexing)
    x = std::max(0.0f, std::min(x, world_w_ - BOUNDARY_EPSILON));
    y = std::max(0.0f, std::min(y, world_h_ - BOUNDARY_EPSILON));

    int idx = cell_index(x, y);
    if (idx >= 0 && idx < static_cast<int>(cells_.size())) {
        cells_[idx].push_back({entity_id, x, y, vx, vy, swarm_type, infected});
    }
}

// Finds all neighbors within a 360 degree radius of a given position.
void SpatialGrid::query_neighbors(
    float x, float y, float radius, std::vector<QueryResult>& results
) const {
    results.clear();

    // Determine cell of query point
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);

    float radius_sq = radius * radius;

    // Dynamic search window: expand beyond 3x3 when radius > cell_size
    int cell_range = static_cast<int>(std::ceil(radius / cell_size_));
    for (int dy = -cell_range; dy <= cell_range; ++dy) {
        for (int dx = -cell_range; dx <= cell_range; ++dx) {
            int check_col = col + dx;
            int check_row = row + dy;

            // Skip cells outside grid bounds
            if (check_col < 0 || check_col >= cols_ ||
                check_row < 0 || check_row >= rows_) {
                continue;
            }

            int idx = check_col + cols_ * check_row;
            if (idx < 0 || idx >= static_cast<int>(cells_.size())) {
                continue;
            }

            // Check all entries in this cell
            for (const auto& entry : cells_[idx]) {
                Vector2 diff = Vector2Subtract({entry.x, entry.y}, {x, y});
                float dist_sq = Vector2LengthSqr(diff);

                if (dist_sq <= radius_sq) {
                    results.push_back({&entry, dist_sq});
                }
            };
        }
    }
}

// Finds all neighbors within a certain radius and field-of-view angle
void SpatialGrid::query_neighbors_fov(
    float x, float y, float radius,
    std::vector<QueryResult>& results,
    float fov, float vx, float vy
) const {
    results.clear();

    // Determine cell of query point
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);

    float radius_sq = radius * radius;

    // Precompute FOV threshold using squared dot-product comparison
    // eliminates per-neighbor sqrt entirely
    float cos_fov = std::cos(fov);
    float vel_mag_sq = vx * vx + vy * vy;
    float cos_fov_sq = cos_fov * cos_fov;
    float fov_threshold = cos_fov_sq * vel_mag_sq;
    bool wide_fov = (cos_fov < 0.0f);  // true when half-angle > pi/2

    // Dynamic search window: expand beyond 3x3 when radius > cell_size
    int cell_range = static_cast<int>(std::ceil(radius / cell_size_));
    for (int dy = -cell_range; dy <= cell_range; ++dy) {
        for (int dx = -cell_range; dx <= cell_range; ++dx) {
            int check_col = col + dx;
            int check_row = row + dy;

            // Skip cells outside grid bounds
            if (check_col < 0 || check_col >= cols_ ||
                check_row < 0 || check_row >= rows_) {
                continue;
            }

            int idx = check_col + cols_ * check_row;
            if (idx < 0 || idx >= static_cast<int>(cells_.size())) {
                continue;
            }

            // Check all entries in this cell
            for (const auto& entry : cells_[idx]) {
                float diff_x = entry.x - x;
                float diff_y = entry.y - y;
                float dist_sq = diff_x * diff_x + diff_y * diff_y;

                if (dist_sq <= radius_sq) { // Within Radius
                    if (vel_mag_sq < 0.00000001f) {
                        // Near-zero velocity: no heading, accept all within radius
                        results.push_back({&entry, dist_sq});
                    } else {
                        float dot = vx * diff_x + vy * diff_y;
                        float dot_sq = dot * dot;
                        float thresh = fov_threshold * dist_sq;
                        // cos_angle >= cos_fov via squared comparison (no sqrt)
                        bool in_fov = wide_fov
                            ? (dot >= 0.0f || dot_sq <= thresh)
                            : (dot >= 0.0f && dot_sq >= thresh);
                        if (in_fov) {
                            results.push_back({&entry, dist_sq});
                        }
                    }
                }
            };
        }
    }
}

int SpatialGrid::cell_index(float x, float y) const {
    // Cell indexing: row-major layout [col + cols_ * row]
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);
    return col + cols_ * row;
}
