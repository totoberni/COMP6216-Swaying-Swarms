#include "spatial_grid.h"
#include "toroidal.h"
#include <cmath>
#include <algorithm>
#include <raymath.h>

namespace {
    // Small epsilon to prevent entities from landing exactly on upper boundary
    constexpr float BOUNDARY_EPSILON = 0.001f;
}

SpatialGrid::SpatialGrid(float world_w, float world_h, float cell_size, bool toroidal)
    : world_w_(world_w)
    , world_h_(world_h)
    , cell_size_(cell_size)
    , cols_(static_cast<int>(std::ceil(world_w / cell_size)))
    , rows_(static_cast<int>(std::ceil(world_h / cell_size)))
    , toroidal_(toroidal)
    , total_cells_(cols_ * rows_)
{
    entries_.resize(1024);
    cell_start_.resize(total_cells_, -1);
    cell_end_.resize(total_cells_, -1);
}

void SpatialGrid::clear() {
    entry_count_ = 0;
    built_ = false;
    std::fill(cell_start_.begin(), cell_start_.end(), -1);
}

void SpatialGrid::insert(uint64_t entity_id, float x, float y,
                          float vx, float vy, uint8_t swarm_type, bool infected) {
    // Clamp to grid bounds (epsilon prevents exact boundary hits causing out-of-bounds indexing)
    x = std::max(0.0f, std::min(x, world_w_ - BOUNDARY_EPSILON));
    y = std::max(0.0f, std::min(y, world_h_ - BOUNDARY_EPSILON));

    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);
    uint32_t cid = static_cast<uint32_t>(col + cols_ * row);

    if (cid >= static_cast<uint32_t>(total_cells_)) return;

    if (static_cast<size_t>(entry_count_) >= entries_.size()) {
        entries_.resize(entries_.size() * 2);
    }
    entries_[entry_count_++] = {entity_id, x, y, vx, vy, swarm_type, infected, cid};
    built_ = false;
}

void SpatialGrid::build() const {
    // Sort entries by cell_id (stable sort preserves insertion order within same cell)
    std::stable_sort(entries_.begin(), entries_.begin() + entry_count_,
        [](const Entry& a, const Entry& b) { return a.cell_id < b.cell_id; });

    // Reset cell_start_ (cell_end_ doesn't need reset — only read when start >= 0)
    std::fill(cell_start_.begin(), cell_start_.end(), -1);

    // Single pass: fill cell_start_/cell_end_ from sorted entries
    for (int i = 0; i < entry_count_; ++i) {
        uint32_t cid = entries_[i].cell_id;
        if (cell_start_[cid] < 0) {
            cell_start_[cid] = i;
        }
        cell_end_[cid] = i + 1;
    }

    built_ = true;
}

// Finds all neighbors within a 360 degree radius of a given position.
void SpatialGrid::query_neighbors(
    float x, float y, float radius, std::vector<QueryResult>& results
) const {
    results.clear();
    if (!built_) build();

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

            if (toroidal_) {
                check_col = ((check_col) % cols_ + cols_) % cols_;
                check_row = ((check_row) % rows_ + rows_) % rows_;
            } else {
                if (check_col < 0 || check_col >= cols_ ||
                    check_row < 0 || check_row >= rows_) {
                    continue;
                }
            }

            int cid = check_col + cols_ * check_row;
            if (cid < 0 || cid >= total_cells_) continue;
            if (cell_start_[cid] < 0) continue;

            for (int i = cell_start_[cid]; i < cell_end_[cid]; ++i) {
                const Entry& entry = entries_[i];
                float dist_sq;
                if (toroidal_) {
                    dist_sq = torus_dist_sq(entry.x, entry.y, x, y, world_w_, world_h_);
                } else {
                    Vector2 diff = Vector2Subtract({entry.x, entry.y}, {x, y});
                    dist_sq = Vector2LengthSqr(diff);
                }

                if (dist_sq <= radius_sq) {
                    results.push_back({&entry, dist_sq});
                }
            }
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
    if (!built_) build();

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

            if (toroidal_) {
                check_col = ((check_col) % cols_ + cols_) % cols_;
                check_row = ((check_row) % rows_ + rows_) % rows_;
            } else {
                if (check_col < 0 || check_col >= cols_ ||
                    check_row < 0 || check_row >= rows_) {
                    continue;
                }
            }

            int cid = check_col + cols_ * check_row;
            if (cid < 0 || cid >= total_cells_) continue;
            if (cell_start_[cid] < 0) continue;

            for (int i = cell_start_[cid]; i < cell_end_[cid]; ++i) {
                const Entry& entry = entries_[i];
                float diff_x, diff_y;
                if (toroidal_) {
                    diff_x = torus_diff(x, entry.x, world_w_);
                    diff_y = torus_diff(y, entry.y, world_h_);
                } else {
                    diff_x = entry.x - x;
                    diff_y = entry.y - y;
                }
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
            }
        }
    }
}
