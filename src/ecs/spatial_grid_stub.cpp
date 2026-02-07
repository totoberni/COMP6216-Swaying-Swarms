// TEMPORARY STUB: SpatialGrid implementation until src/spatial/ module is complete
#include "spatial_grid.h"

SpatialGrid::SpatialGrid(float world_w, float world_h, float cell_size)
    : world_w_(world_w), world_h_(world_h), cell_size_(cell_size) {
    cols_ = static_cast<int>(world_w / cell_size) + 1;
    rows_ = static_cast<int>(world_h / cell_size) + 1;
    cells_.resize(cols_ * rows_);
}

void SpatialGrid::clear() {
    for (auto& cell : cells_) {
        cell.clear();
    }
}

void SpatialGrid::insert(uint64_t entity_id, float x, float y) {
    int idx = cell_index(x, y);
    if (idx >= 0 && idx < static_cast<int>(cells_.size())) {
        cells_[idx].push_back(Entry{entity_id, x, y});
    }
}

std::vector<std::pair<uint64_t, float>> SpatialGrid::query_neighbors(float x, float y, float radius) const {
    // Stub: return empty for now
    return {};
}

int SpatialGrid::cell_index(float x, float y) const {
    int col = static_cast<int>(x / cell_size_);
    int row = static_cast<int>(y / cell_size_);
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) {
        return -1;
    }
    return row * cols_ + col;
}
