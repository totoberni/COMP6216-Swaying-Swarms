#include "spatial_grid.h"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <unordered_set>
#include <cmath>

class SpatialGridTest : public ::testing::Test {
protected:
    static constexpr float WORLD_W = 800.0f;
    static constexpr float WORLD_H = 600.0f;
    static constexpr float CELL_SIZE = 50.0f;
};

TEST_F(SpatialGridTest, EmptyGridReturnsNoNeighbors) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(100.0f, 100.0f, 50.0f, neighbors);

    EXPECT_TRUE(neighbors.empty());
}

TEST_F(SpatialGridTest, SingleEntityFoundWithinRadius) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(110.0f, 110.0f, 20.0f, neighbors);

    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].entry->entity_id, 1);

    // dist_sq should be approximately 10^2 + 10^2 = 200
    EXPECT_NEAR(neighbors[0].dist_sq, 200.0f, 0.1f);
}

TEST_F(SpatialGridTest, EntityOutsideRadiusNotReturned) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(200.0f, 200.0f, 50.0f, neighbors);

    EXPECT_TRUE(neighbors.empty());
}

TEST_F(SpatialGridTest, MultipleEntitiesFoundWithinRadius) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);  // Distance 0
    grid.insert(2, 110.0f, 100.0f);  // Distance 10
    grid.insert(3, 120.0f, 100.0f);  // Distance 20
    grid.insert(4, 130.0f, 100.0f);  // Distance 30

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(100.0f, 100.0f, 25.0f, neighbors);

    // Should find 3 entities within 25px (0, 10, 20 — not 30)
    ASSERT_EQ(neighbors.size(), 3);

    // Results are NOT sorted — verify all 3 are present
    std::unordered_set<uint64_t> ids;
    for (const auto& qr : neighbors) {
        ids.insert(qr.entry->entity_id);
    }
    EXPECT_TRUE(ids.count(1));
    EXPECT_TRUE(ids.count(2));
    EXPECT_TRUE(ids.count(3));
}

TEST_F(SpatialGridTest, BoundaryEntitiesHandledCorrectly) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Insert entities at boundaries
    grid.insert(1, 0.0f, 0.0f);           // Top-left corner
    grid.insert(2, WORLD_W - 1, 0.0f);    // Top-right corner
    grid.insert(3, 0.0f, WORLD_H - 1);    // Bottom-left corner
    grid.insert(4, WORLD_W - 1, WORLD_H - 1); // Bottom-right corner

    std::vector<SpatialGrid::QueryResult> neighbors;

    // Query at each corner
    grid.query_neighbors(0.0f, 0.0f, 10.0f, neighbors);
    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].entry->entity_id, 1);

    grid.query_neighbors(WORLD_W - 1, 0.0f, 10.0f, neighbors);
    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].entry->entity_id, 2);

    grid.query_neighbors(0.0f, WORLD_H - 1, 10.0f, neighbors);
    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].entry->entity_id, 3);

    grid.query_neighbors(WORLD_W - 1, WORLD_H - 1, 10.0f, neighbors);
    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].entry->entity_id, 4);
}

TEST_F(SpatialGridTest, EntitiesOutOfBoundsAreClamped) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Try to insert entities outside bounds
    grid.insert(1, -10.0f, -10.0f);
    grid.insert(2, WORLD_W + 100, WORLD_H + 100);

    std::vector<SpatialGrid::QueryResult> neighbors;

    // Should be clamped to edges
    grid.query_neighbors(0.0f, 0.0f, 5.0f, neighbors);
    EXPECT_EQ(neighbors.size(), 1);

    grid.query_neighbors(WORLD_W - 1, WORLD_H - 1, 5.0f, neighbors);
    EXPECT_EQ(neighbors.size(), 1);
}

TEST_F(SpatialGridTest, ClearResetsGrid) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);
    grid.insert(2, 110.0f, 110.0f);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(100.0f, 100.0f, 50.0f, neighbors);
    EXPECT_EQ(neighbors.size(), 2);

    grid.clear();

    grid.query_neighbors(100.0f, 100.0f, 50.0f, neighbors);
    EXPECT_TRUE(neighbors.empty());
}

TEST_F(SpatialGridTest, LargeScaleCorrectness) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Create 10000 entities with known positions
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist_x(0.0f, WORLD_W);
    std::uniform_real_distribution<float> dist_y(0.0f, WORLD_H);

    struct EntityPos {
        uint64_t id;
        float x, y;
    };

    std::vector<EntityPos> entities;
    for (uint64_t i = 0; i < 10000; ++i) {
        float x = dist_x(rng);
        float y = dist_y(rng);
        entities.push_back({i, x, y});
        grid.insert(i, x, y);
    }

    // Test query point
    float query_x = 400.0f;
    float query_y = 300.0f;
    float query_radius = 50.0f;

    // Get results from spatial grid
    std::vector<SpatialGrid::QueryResult> grid_results;
    grid.query_neighbors(query_x, query_y, query_radius, grid_results);

    // Brute-force verification
    std::vector<std::pair<uint64_t, float>> brute_force;
    for (const auto& entity : entities) {
        float dx = entity.x - query_x;
        float dy = entity.y - query_y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= query_radius) {
            brute_force.push_back({entity.id, dist});
        }
    }

    // Same count
    ASSERT_EQ(grid_results.size(), brute_force.size());

    // Verify all brute-force IDs are in grid results
    std::unordered_set<uint64_t> grid_ids;
    for (const auto& qr : grid_results) {
        grid_ids.insert(qr.entry->entity_id);
    }
    for (const auto& [id, d] : brute_force) {
        EXPECT_TRUE(grid_ids.count(id)) << "Missing entity " << id;
    }
}

TEST_F(SpatialGridTest, PerformanceTest) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    std::mt19937 rng(54321);
    std::uniform_real_distribution<float> dist_x(0.0f, WORLD_W);
    std::uniform_real_distribution<float> dist_y(0.0f, WORLD_H);

    auto start = std::chrono::high_resolution_clock::now();

    // Insert 10000 entities
    for (uint64_t i = 0; i < 10000; ++i) {
        grid.insert(i, dist_x(rng), dist_y(rng));
    }

    // Perform 1000 queries with reused scratch vector
    std::vector<SpatialGrid::QueryResult> neighbors;
    for (int i = 0; i < 1000; ++i) {
        grid.query_neighbors(dist_x(rng), dist_y(rng), 50.0f, neighbors);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in less than 100ms (relaxed for WSL2 timing variance)
    EXPECT_LT(duration.count(), 100);

    // Print actual time for debugging
    std::cout << "Performance test completed in " << duration.count() << "ms" << std::endl;
}

TEST_F(SpatialGridTest, CrossCellBoundaryQueries) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Insert entities on both sides of a cell boundary
    // Assuming CELL_SIZE = 50.0f, boundary at x=50.0f
    grid.insert(1, 48.0f, 100.0f);  // Just before boundary
    grid.insert(2, 52.0f, 100.0f);  // Just after boundary

    // Query from just before boundary
    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(49.0f, 100.0f, 10.0f, neighbors);

    // Should find both entities (1 is 1 unit away, 2 is 3 units away)
    ASSERT_EQ(neighbors.size(), 2);
}

TEST_F(SpatialGridTest, QueryPointOutsideBoundsHandledCorrectly) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 10.0f, 10.0f);
    grid.insert(2, 50.0f, 50.0f);

    std::vector<SpatialGrid::QueryResult> neighbors;

    // Query from outside bounds (negative coordinates)
    grid.query_neighbors(-50.0f, -50.0f, 100.0f, neighbors);
    EXPECT_EQ(neighbors.size(), 1);  // Should find entity at (10, 10)
    EXPECT_EQ(neighbors[0].entry->entity_id, 1);

    // Query from outside bounds (beyond world dimensions)
    grid.query_neighbors(WORLD_W + 100, WORLD_H + 100, 100.0f, neighbors);
    EXPECT_TRUE(neighbors.empty());  // Too far from any entity

    // Query from outside with large radius
    grid.query_neighbors(-10.0f, -10.0f, 30.0f, neighbors);
    EXPECT_EQ(neighbors.size(), 1);  // Distance to (10, 10) is ~28.28
    EXPECT_EQ(neighbors[0].entry->entity_id, 1);
}

TEST_F(SpatialGridTest, QueryRadiusLargerThanCellSize) {
    // Cell size = 50, but query radius = 150. Dynamic window must expand.
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Place entity 120px away from query point (beyond 3x3 cell window = 150px max)
    grid.insert(1, 100.0f, 100.0f);
    grid.insert(2, 220.0f, 100.0f);  // 120px away from (100,100)

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(100.0f, 100.0f, 150.0f, neighbors);

    // Both should be found: self at 0, other at 120
    ASSERT_EQ(neighbors.size(), 2);

    // Find entity 2 in results (order not guaranteed)
    bool found_entity2 = false;
    for (const auto& qr : neighbors) {
        if (qr.entry->entity_id == 2) {
            EXPECT_NEAR(qr.dist_sq, 14400.0f, 0.1f);  // 120^2 = 14400
            found_entity2 = true;
        }
    }
    EXPECT_TRUE(found_entity2);
}

TEST_F(SpatialGridTest, LargeRadiusBruteForceComparison) {
    // Stress test: radius=200, cell_size=50 — requires 4-cell range
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    std::mt19937 rng(99999);
    std::uniform_real_distribution<float> dist_x(0.0f, WORLD_W);
    std::uniform_real_distribution<float> dist_y(0.0f, WORLD_H);

    struct EntityPos { uint64_t id; float x, y; };
    std::vector<EntityPos> entities;
    for (uint64_t i = 0; i < 10000; ++i) {
        float x = dist_x(rng);
        float y = dist_y(rng);
        entities.push_back({i, x, y});
        grid.insert(i, x, y);
    }

    float qx = 400.0f, qy = 300.0f, qr = 200.0f;
    std::vector<SpatialGrid::QueryResult> grid_results;
    grid.query_neighbors(qx, qy, qr, grid_results);

    // Brute-force verification
    std::vector<std::pair<uint64_t, float>> brute_force;
    for (const auto& e : entities) {
        float dx = e.x - qx, dy = e.y - qy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= qr) brute_force.push_back({e.id, d});
    }

    ASSERT_EQ(grid_results.size(), brute_force.size());

    // Verify all brute-force IDs appear in grid results
    std::unordered_set<uint64_t> grid_ids;
    for (const auto& qr_item : grid_results) {
        grid_ids.insert(qr_item.entry->entity_id);
    }
    for (const auto& [id, d] : brute_force) {
        EXPECT_TRUE(grid_ids.count(id)) << "Missing entity " << id;
    }
}

TEST_F(SpatialGridTest, EnrichedEntryFields) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Insert with enriched fields
    grid.insert(42, 100.0f, 100.0f, 1.5f, -2.5f, 1, false);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(100.0f, 100.0f, 10.0f, neighbors);

    ASSERT_EQ(neighbors.size(), 1);
    const auto* entry = neighbors[0].entry;
    EXPECT_EQ(entry->entity_id, 42);
    EXPECT_FLOAT_EQ(entry->vx, 1.5f);
    EXPECT_FLOAT_EQ(entry->vy, -2.5f);
    EXPECT_EQ(entry->swarm_type, 1);
    EXPECT_TRUE(entry->infected);
}

TEST_F(SpatialGridTest, BackwardCompatInsertUsesDefaults) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // 3-arg insert should set defaults
    grid.insert(7, 200.0f, 300.0f);

    std::vector<SpatialGrid::QueryResult> neighbors;
    grid.query_neighbors(200.0f, 300.0f, 10.0f, neighbors);

    ASSERT_EQ(neighbors.size(), 1);
    const auto* entry = neighbors[0].entry;
    EXPECT_EQ(entry->entity_id, 7);
    EXPECT_FLOAT_EQ(entry->vx, 0.0f);
    EXPECT_FLOAT_EQ(entry->vy, 0.0f);
    EXPECT_EQ(entry->swarm_type, 0);
    EXPECT_FALSE(entry->infected);
}
