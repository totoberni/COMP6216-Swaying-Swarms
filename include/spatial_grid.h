#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <memory>

class SpatialGrid {
public:
    // --- Enriched entry stored in flat sorted array ---
    struct Entry {
        uint64_t entity_id;
        float x, y;
        float vx, vy;         // velocity for alignment
        uint8_t swarm_type;    // 0=normal, 1=doctor
        bool infected;
        uint32_t cell_id;      // grid cell index (for sort-based layout)
    };

    // --- Query result: pointer to entry + squared distance ---
    struct QueryResult {
        const Entry* entry;   // pointer into grid storage (valid until next clear())
        float dist_sq;        // squared distance (caller takes sqrt only if needed)
    };

    SpatialGrid() = default;
    SpatialGrid(float world_w, float world_h, float cell_size, bool toroidal = false);

    void clear();

    // Full insert with enriched fields
    void insert(uint64_t entity_id, float x, float y,
                float vx, float vy, uint8_t swarm_type, bool infected);

    // Convenience overload: no extra fields (backward compat for tests)
    void insert(uint64_t entity_id, float x, float y) {
        insert(entity_id, x, y, 0.0f, 0.0f, 0, false);
    }

    // Fills `results` with QueryResult entries within radius. Not sorted.
    // Caller should reuse the vector for amortized zero-allocation queries.
    // Search window expands dynamically: ceil(radius / cell_size) cells in each direction.
    // dist_sq is squared distance — caller takes sqrt only when needed.
    void query_neighbors(
        float x, float y, float radius,
        std::vector<QueryResult>& results
    ) const;

    void query_neighbors_fov(
        float x, float y, float radius,
        std::vector<QueryResult>& results,
        float fov, float vx, float vy
    ) const;

    // Returns the cell size used for spatial partitioning (diagnostic use).
    float cell_size() const { return cell_size_; }

private:
    float world_w_ = 0.0f;
    float world_h_ = 0.0f;
    float cell_size_ = 1.0f;
    int cols_ = 0;
    int rows_ = 0;
    bool toroidal_ = false;
    int total_cells_ = 0;

    // Sort-based flat array (mutable for lazy build from const queries)
    mutable std::vector<Entry> entries_;
    mutable std::vector<int> cell_start_;
    mutable std::vector<int> cell_end_;
    mutable int entry_count_ = 0;
    mutable bool built_ = false;

    void build() const;
};
