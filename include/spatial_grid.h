#pragma once

#include <cstdint>
#include <vector>
#include <utility>

class SpatialGrid {
public:
    SpatialGrid(float world_w, float world_h, float cell_size);

    void clear();
    void insert(uint64_t entity_id, float x, float y);

    // Returns (entity_id, distance) pairs sorted by distance ascending
    std::vector<std::pair<uint64_t, float>> query_neighbors(float x, float y, float radius) const;

private:
    float world_w_;
    float world_h_;
    float cell_size_;
    int cols_;
    int rows_;

    struct Entry {
        uint64_t entity_id;
        float x, y;
    };

    std::vector<std::vector<Entry>> cells_;

    int cell_index(float x, float y) const;
};
