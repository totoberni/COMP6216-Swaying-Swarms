#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <memory>

class SpatialGrid {
public:
    // --- Flag constants for Entry::flags bitfield ---
    static constexpr uint8_t FLAG_INFECTED = 0x01;
    static constexpr uint8_t FLAG_ALIVE    = 0x02;
    static constexpr uint8_t FLAG_MALE     = 0x04;

    // --- Enriched entry stored in each grid cell ---
    struct Entry {
        uint64_t entity_id;
        float x, y;
        float vx, vy;         // velocity for alignment
        uint8_t swarm_type;    // 0=normal, 1=doctor, 2=antivax
        uint8_t flags;         // bit 0=infected, bit 1=alive, bit 2=male
    };

    // --- Query result: pointer to entry + squared distance ---
    struct QueryResult {
        const Entry* entry;   // pointer into grid storage (valid until next clear())
        float dist_sq;        // squared distance (caller takes sqrt only if needed)
    };

    SpatialGrid() = default;
    SpatialGrid(float world_w, float world_h, float cell_size);

    void clear();

    // Full insert with enriched fields
    void insert(uint64_t entity_id, float x, float y,
                float vx, float vy, uint8_t swarm_type, uint8_t flags);

    // Convenience overload: no extra fields (backward compat for tests)
    void insert(uint64_t entity_id, float x, float y) {
        insert(entity_id, x, y, 0.0f, 0.0f, 0, FLAG_ALIVE);
    }

    // Fills `results` with QueryResult entries within radius. Not sorted.
    // Caller should reuse the vector for amortized zero-allocation queries.
    // Search window expands dynamically: ceil(radius / cell_size) cells in each direction.
    // dist_sq is squared distance — caller takes sqrt only when needed.
    void query_neighbors(float x, float y, float radius,
                         std::vector<QueryResult>& results) const;

    // Returns the cell size used for spatial partitioning (diagnostic use).
    float cell_size() const { return cell_size_; }

private:
    float world_w_ = 0.0f;
    float world_h_ = 0.0f;
    float cell_size_ = 1.0f;
    int cols_ = 0;
    int rows_ = 0;

    std::vector<std::vector<Entry>> cells_;

    int cell_index(float x, float y) const;
};
