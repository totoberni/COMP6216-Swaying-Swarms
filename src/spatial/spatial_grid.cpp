#include "spatial_grid.h"
#include <cmath>
#include <algorithm>

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

void SpatialGrid::insert(uint64_t entity_id, float x, float y) {
    // Clamp to grid bounds
    x = std::max(0.0f, std::min(x, world_w_ - 0.001f));
    y = std::max(0.0f, std::min(y, world_h_ - 0.001f));

    int idx = cell_index(x, y);
    if (idx >= 0 && idx < static_cast<int>(cells_.size())) {
        cells_[idx].push_back({entity_id, x, y});
    }
}

std::vector<std::pair<uint64_t, float>> SpatialGrid::query_neighbors(float x, float y, float radius) const {
    std::vector<std::pair<uint64_t, float>> results;

    // Determine cell range to check (current cell + 8 neighbors)
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);

    float radius_sq = radius * radius;

    // Check 3x3 grid centered on current cell
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
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
                float dx_val = entry.x - x;
                float dy_val = entry.y - y;
                float dist_sq = dx_val * dx_val + dy_val * dy_val;

                if (dist_sq <= radius_sq) {
                    results.push_back({entry.entity_id, std::sqrt(dist_sq)});
                }
            }
        }
    }

    // Sort by distance ascending
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return results;
}

int SpatialGrid::cell_index(float x, float y) const {
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);
    return col + cols_ * row;
}
