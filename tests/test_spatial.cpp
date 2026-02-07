#include "spatial_grid.h"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <unordered_set>

class SpatialGridTest : public ::testing::Test {
protected:
    static constexpr float WORLD_W = 800.0f;
    static constexpr float WORLD_H = 600.0f;
    static constexpr float CELL_SIZE = 50.0f;
};

TEST_F(SpatialGridTest, EmptyGridReturnsNoNeighbors) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    auto neighbors = grid.query_neighbors(100.0f, 100.0f, 50.0f);

    EXPECT_TRUE(neighbors.empty());
}

TEST_F(SpatialGridTest, SingleEntityFoundWithinRadius) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);

    auto neighbors = grid.query_neighbors(110.0f, 110.0f, 20.0f);

    ASSERT_EQ(neighbors.size(), 1);
    EXPECT_EQ(neighbors[0].first, 1);

    // Distance should be approximately sqrt(10^2 + 10^2) = sqrt(200) ≈ 14.14
    EXPECT_NEAR(neighbors[0].second, 14.142f, 0.01f);
}

TEST_F(SpatialGridTest, EntityOutsideRadiusNotReturned) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);

    auto neighbors = grid.query_neighbors(200.0f, 200.0f, 50.0f);

    EXPECT_TRUE(neighbors.empty());
}

TEST_F(SpatialGridTest, MultipleEntitiesSortedByDistance) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);  // Distance 0
    grid.insert(2, 110.0f, 100.0f);  // Distance 10
    grid.insert(3, 120.0f, 100.0f);  // Distance 20
    grid.insert(4, 130.0f, 100.0f);  // Distance 30

    auto neighbors = grid.query_neighbors(100.0f, 100.0f, 25.0f);

    ASSERT_EQ(neighbors.size(), 3);
    EXPECT_EQ(neighbors[0].first, 1);
    EXPECT_EQ(neighbors[1].first, 2);
    EXPECT_EQ(neighbors[2].first, 3);

    // Verify sorted by distance
    EXPECT_LT(neighbors[0].second, neighbors[1].second);
    EXPECT_LT(neighbors[1].second, neighbors[2].second);
}

TEST_F(SpatialGridTest, BoundaryEntitiesHandledCorrectly) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Insert entities at boundaries
    grid.insert(1, 0.0f, 0.0f);           // Top-left corner
    grid.insert(2, WORLD_W - 1, 0.0f);    // Top-right corner
    grid.insert(3, 0.0f, WORLD_H - 1);    // Bottom-left corner
    grid.insert(4, WORLD_W - 1, WORLD_H - 1); // Bottom-right corner

    // Query at each corner
    auto neighbors1 = grid.query_neighbors(0.0f, 0.0f, 10.0f);
    ASSERT_EQ(neighbors1.size(), 1);
    EXPECT_EQ(neighbors1[0].first, 1);

    auto neighbors2 = grid.query_neighbors(WORLD_W - 1, 0.0f, 10.0f);
    ASSERT_EQ(neighbors2.size(), 1);
    EXPECT_EQ(neighbors2[0].first, 2);

    auto neighbors3 = grid.query_neighbors(0.0f, WORLD_H - 1, 10.0f);
    ASSERT_EQ(neighbors3.size(), 1);
    EXPECT_EQ(neighbors3[0].first, 3);

    auto neighbors4 = grid.query_neighbors(WORLD_W - 1, WORLD_H - 1, 10.0f);
    ASSERT_EQ(neighbors4.size(), 1);
    EXPECT_EQ(neighbors4[0].first, 4);
}

TEST_F(SpatialGridTest, EntitiesOutOfBoundsAreClamped) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    // Try to insert entities outside bounds
    grid.insert(1, -10.0f, -10.0f);
    grid.insert(2, WORLD_W + 100, WORLD_H + 100);

    // Should be clamped to edges
    auto neighbors1 = grid.query_neighbors(0.0f, 0.0f, 5.0f);
    EXPECT_EQ(neighbors1.size(), 1);

    auto neighbors2 = grid.query_neighbors(WORLD_W - 1, WORLD_H - 1, 5.0f);
    EXPECT_EQ(neighbors2.size(), 1);
}

TEST_F(SpatialGridTest, ClearResetsGrid) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 100.0f, 100.0f);
    grid.insert(2, 110.0f, 110.0f);

    auto neighbors_before = grid.query_neighbors(100.0f, 100.0f, 50.0f);
    EXPECT_EQ(neighbors_before.size(), 2);

    grid.clear();

    auto neighbors_after = grid.query_neighbors(100.0f, 100.0f, 50.0f);
    EXPECT_TRUE(neighbors_after.empty());
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
    auto grid_results = grid.query_neighbors(query_x, query_y, query_radius);

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
    std::sort(brute_force.begin(), brute_force.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Results should match
    ASSERT_EQ(grid_results.size(), brute_force.size());

    for (size_t i = 0; i < grid_results.size(); ++i) {
        EXPECT_EQ(grid_results[i].first, brute_force[i].first);
        EXPECT_NEAR(grid_results[i].second, brute_force[i].second, 0.001f);
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

    // Perform 1000 queries
    for (int i = 0; i < 1000; ++i) {
        grid.query_neighbors(dist_x(rng), dist_y(rng), 50.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in less than 50ms
    EXPECT_LT(duration.count(), 50);

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
    auto neighbors = grid.query_neighbors(49.0f, 100.0f, 10.0f);

    // Should find both entities (1 is 1 unit away, 2 is 3 units away)
    ASSERT_EQ(neighbors.size(), 2);
    EXPECT_EQ(neighbors[0].first, 1);
    EXPECT_EQ(neighbors[1].first, 2);
}

TEST_F(SpatialGridTest, QueryPointOutsideBoundsHandledCorrectly) {
    SpatialGrid grid(WORLD_W, WORLD_H, CELL_SIZE);

    grid.insert(1, 10.0f, 10.0f);
    grid.insert(2, 50.0f, 50.0f);

    // Query from outside bounds (negative coordinates)
    auto neighbors1 = grid.query_neighbors(-50.0f, -50.0f, 100.0f);
    EXPECT_EQ(neighbors1.size(), 1);  // Should find entity at (10, 10)
    EXPECT_EQ(neighbors1[0].first, 1);

    // Query from outside bounds (beyond world dimensions)
    auto neighbors2 = grid.query_neighbors(WORLD_W + 100, WORLD_H + 100, 100.0f);
    EXPECT_TRUE(neighbors2.empty());  // Too far from any entity

    // Query from outside with large radius
    auto neighbors3 = grid.query_neighbors(-10.0f, -10.0f, 30.0f);
    EXPECT_EQ(neighbors3.size(), 1);  // Distance to (10, 10) is ~28.28
    EXPECT_EQ(neighbors3[0].first, 1);
}
