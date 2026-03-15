#include <gtest/gtest.h>
#include <cuda_runtime.h>
#include "gpu/gpu_buffers.h"
#include "gpu/spatial_hash.h"
#include "spatial_grid.h"
#include <vector>
#include <random>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <set>

// --- Host-side cell_id computation (mirrors GPU calcHash) ---
static uint32_t host_cell_id(float x, float y, float cell_size, int cols, int rows) {
    int col = std::max(0, std::min(static_cast<int>(x / cell_size), cols - 1));
    int row = std::max(0, std::min(static_cast<int>(y / cell_size), rows - 1));
    return static_cast<uint32_t>(col + cols * row);
}

// --- SoA boid data on host ---
struct HostBoids {
    std::vector<float> pos_x, pos_y, vel_x, vel_y, immunity;
    std::vector<uint8_t> swarm_type, infected;
};

static HostBoids generate_boids(int n, float world_w, float world_h, uint32_t seed) {
    HostBoids b;
    b.pos_x.resize(n); b.pos_y.resize(n);
    b.vel_x.resize(n); b.vel_y.resize(n);
    b.swarm_type.resize(n); b.infected.resize(n);
    b.immunity.resize(n);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dx(0.01f, world_w - 0.01f);
    std::uniform_real_distribution<float> dy(0.01f, world_h - 0.01f);
    std::uniform_real_distribution<float> dv(-5.0f, 5.0f);

    for (int i = 0; i < n; i++) {
        b.pos_x[i] = dx(rng);
        b.pos_y[i] = dy(rng);
        b.vel_x[i] = dv(rng);
        b.vel_y[i] = dv(rng);
        b.swarm_type[i] = (i % 10 == 0) ? 1 : 0;
        b.infected[i] = (i % 5 == 0) ? 1 : 0;
        b.immunity[i] = 0.0f;
    }
    return b;
}

// --- Verify GPU hash: each sorted position maps to its cell, total count preserved ---
static void verify_gpu_hash(
    const std::vector<int>& cell_start, const std::vector<int>& cell_end,
    const std::vector<float>& sorted_px, const std::vector<float>& sorted_py,
    int count, float cell_size, int cols, int rows, int total_cells)
{
    int total = 0;
    for (int cid = 0; cid < total_cells; cid++) {
        if (cell_start[cid] < 0) continue;
        ASSERT_GE(cell_start[cid], 0);
        ASSERT_LE(cell_end[cid], count);
        ASSERT_LE(cell_start[cid], cell_end[cid]);
        for (int i = cell_start[cid]; i < cell_end[cid]; i++) {
            uint32_t expected = host_cell_id(sorted_px[i], sorted_py[i],
                                             cell_size, cols, rows);
            ASSERT_EQ(expected, static_cast<uint32_t>(cid))
                << "Boid at (" << sorted_px[i] << ", " << sorted_py[i]
                << ") in cell " << cid << " but should be in " << expected;
            total++;
        }
    }
    ASSERT_EQ(total, count) << "Not all boids accounted for in spatial hash";
}

// --- Compare GPU cell counts against host-computed reference ---
static void compare_cell_counts(
    const HostBoids& data, int count,
    const std::vector<int>& cell_start, const std::vector<int>& cell_end,
    float cell_size, int cols, int rows, int total_cells)
{
    std::vector<int> ref_count(total_cells, 0);
    for (int i = 0; i < count; i++) {
        uint32_t cid = host_cell_id(data.pos_x[i], data.pos_y[i],
                                    cell_size, cols, rows);
        ref_count[cid]++;
    }
    for (int cid = 0; cid < total_cells; cid++) {
        int gpu_count = (cell_start[cid] >= 0)
                            ? (cell_end[cid] - cell_start[cid]) : 0;
        ASSERT_EQ(gpu_count, ref_count[cid])
            << "Cell " << cid << ": GPU=" << gpu_count
            << " ref=" << ref_count[cid];
    }
}

// --- Compare GPU cell-position sets against CPU SpatialGrid ---
static void compare_with_cpu_grid(
    const HostBoids& data, int count,
    const std::vector<int>& cell_start, const std::vector<int>& cell_end,
    const std::vector<float>& sorted_px, const std::vector<float>& sorted_py,
    float world_w, float world_h, float cell_size, int cols, int rows, int total_cells)
{
    // Build CPU grid reference: cell_id -> sorted list of (x, y) pairs
    std::vector<std::vector<std::pair<float,float>>> cpu_cells(total_cells);
    for (int i = 0; i < count; i++) {
        uint32_t cid = host_cell_id(data.pos_x[i], data.pos_y[i],
                                    cell_size, cols, rows);
        cpu_cells[cid].emplace_back(data.pos_x[i], data.pos_y[i]);
    }
    for (auto& v : cpu_cells) {
        std::sort(v.begin(), v.end());
    }

    // Build GPU reference from downloaded sorted arrays
    std::vector<std::vector<std::pair<float,float>>> gpu_cells(total_cells);
    for (int cid = 0; cid < total_cells; cid++) {
        if (cell_start[cid] < 0) continue;
        for (int i = cell_start[cid]; i < cell_end[cid]; i++) {
            gpu_cells[cid].emplace_back(sorted_px[i], sorted_py[i]);
        }
        std::sort(gpu_cells[cid].begin(), gpu_cells[cid].end());
    }

    // Compare cell by cell
    for (int cid = 0; cid < total_cells; cid++) {
        ASSERT_EQ(cpu_cells[cid].size(), gpu_cells[cid].size())
            << "Cell " << cid << " size mismatch";
        for (size_t j = 0; j < cpu_cells[cid].size(); j++) {
            ASSERT_NEAR(cpu_cells[cid][j].first,  gpu_cells[cid][j].first,  1e-6f)
                << "Cell " << cid << " entry " << j << " x mismatch";
            ASSERT_NEAR(cpu_cells[cid][j].second, gpu_cells[cid][j].second, 1e-6f)
                << "Cell " << cid << " entry " << j << " y mismatch";
        }
    }
}

// --- Helper: run GPU build and download results ---
struct GpuResult {
    std::vector<int> cell_start, cell_end;
    std::vector<float> sorted_px, sorted_py;
};

static GpuResult run_gpu_build(const HostBoids& data, int n, const GridParams& grid) {
    GpuBuffers buf;
    gpu_buffers_alloc(buf, n, grid.total_cells);
    gpu_buffers_upload(buf, n,
        data.pos_x.data(), data.pos_y.data(),
        data.vel_x.data(), data.vel_y.data(),
        data.swarm_type.data(), data.infected.data(), data.immunity.data());

    gpu_spatial_hash_build(buf, grid);

    GpuResult r;
    r.cell_start.resize(grid.total_cells);
    r.cell_end.resize(grid.total_cells);
    r.sorted_px.resize(n);
    r.sorted_py.resize(n);

    cudaMemcpy(r.cell_start.data(), buf.d_cell_start,
               grid.total_cells * sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(r.cell_end.data(), buf.d_cell_end,
               grid.total_cells * sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(r.sorted_px.data(), buf.d_pos_x,
               n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(r.sorted_py.data(), buf.d_pos_y,
               n * sizeof(float), cudaMemcpyDeviceToHost);

    gpu_buffers_free(buf);
    return r;
}

// =====================================================================
// Tests
// =====================================================================

TEST(GpuSpatialHash, Small100) {
    const int N = 100;
    const float W = 500.0f, H = 500.0f, CS = 50.0f;
    const int cols = static_cast<int>(std::ceil(W / CS));
    const int rows = static_cast<int>(std::ceil(H / CS));
    const int tc = cols * rows;

    auto data = generate_boids(N, W, H, 42);
    GridParams grid{W, H, CS, cols, rows, tc, false};
    auto r = run_gpu_build(data, N, grid);

    verify_gpu_hash(r.cell_start, r.cell_end, r.sorted_px, r.sorted_py,
                    N, CS, cols, rows, tc);
    compare_cell_counts(data, N, r.cell_start, r.cell_end, CS, cols, rows, tc);
    compare_with_cpu_grid(data, N, r.cell_start, r.cell_end,
                          r.sorted_px, r.sorted_py, W, H, CS, cols, rows, tc);
}

TEST(GpuSpatialHash, Medium10k) {
    const int N = 10000;
    const float W = 13500.0f, H = 13500.0f, CS = 30.0f;
    const int cols = static_cast<int>(std::ceil(W / CS));
    const int rows = static_cast<int>(std::ceil(H / CS));
    const int tc = cols * rows;

    auto data = generate_boids(N, W, H, 123);
    GridParams grid{W, H, CS, cols, rows, tc, false};
    auto r = run_gpu_build(data, N, grid);

    verify_gpu_hash(r.cell_start, r.cell_end, r.sorted_px, r.sorted_py,
                    N, CS, cols, rows, tc);
    compare_cell_counts(data, N, r.cell_start, r.cell_end, CS, cols, rows, tc);
    compare_with_cpu_grid(data, N, r.cell_start, r.cell_end,
                          r.sorted_px, r.sorted_py, W, H, CS, cols, rows, tc);
}

TEST(GpuSpatialHash, Perf100k) {
    const int N = 100000;
    const float W = 13500.0f, H = 13500.0f, CS = 30.0f;
    const int cols = static_cast<int>(std::ceil(W / CS));
    const int rows = static_cast<int>(std::ceil(H / CS));
    const int tc = cols * rows;

    auto data = generate_boids(N, W, H, 777);

    GpuBuffers buf;
    gpu_buffers_alloc(buf, N, tc);
    gpu_buffers_upload(buf, N,
        data.pos_x.data(), data.pos_y.data(),
        data.vel_x.data(), data.vel_y.data(),
        data.swarm_type.data(), data.infected.data(), data.immunity.data());

    // Warm-up
    GridParams grid{W, H, CS, cols, rows, tc, false};
    gpu_spatial_hash_build(buf, grid);

    // Re-upload (swap undone by re-upload overwriting primary arrays)
    gpu_buffers_upload(buf, N,
        data.pos_x.data(), data.pos_y.data(),
        data.vel_x.data(), data.vel_y.data(),
        data.swarm_type.data(), data.infected.data(), data.immunity.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    gpu_spatial_hash_build(buf, grid);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::printf("[  TIMING ] 100k spatial hash build: %.2f ms\n", ms);
    ASSERT_LT(ms, 50.0) << "100k build exceeded 50ms budget";

    // Quick correctness check on the timed run
    std::vector<int> h_cs(tc), h_ce(tc);
    std::vector<float> h_px(N), h_py(N);
    cudaMemcpy(h_cs.data(), buf.d_cell_start, tc * sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_ce.data(), buf.d_cell_end, tc * sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_px.data(), buf.d_pos_x, N * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_py.data(), buf.d_pos_y, N * sizeof(float), cudaMemcpyDeviceToHost);

    verify_gpu_hash(h_cs, h_ce, h_px, h_py, N, CS, cols, rows, tc);

    gpu_buffers_free(buf);
}

TEST(GpuSpatialHash, Perf1M) {
    const int N = 1000000;
    const float W = 13500.0f, H = 13500.0f, CS = 30.0f;
    const int cols = static_cast<int>(std::ceil(W / CS));
    const int rows = static_cast<int>(std::ceil(H / CS));
    const int tc = cols * rows;

    auto data = generate_boids(N, W, H, 999);

    GpuBuffers buf;
    gpu_buffers_alloc(buf, N, tc);
    gpu_buffers_upload(buf, N,
        data.pos_x.data(), data.pos_y.data(),
        data.vel_x.data(), data.vel_y.data(),
        data.swarm_type.data(), data.infected.data(), data.immunity.data());

    // Warm-up
    GridParams grid{W, H, CS, cols, rows, tc, false};
    gpu_spatial_hash_build(buf, grid);

    // Re-upload for timed run
    gpu_buffers_upload(buf, N,
        data.pos_x.data(), data.pos_y.data(),
        data.vel_x.data(), data.vel_y.data(),
        data.swarm_type.data(), data.infected.data(), data.immunity.data());

    auto t0 = std::chrono::high_resolution_clock::now();
    gpu_spatial_hash_build(buf, grid);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::printf("[  TIMING ] 1M spatial hash build: %.2f ms\n", ms);
    ASSERT_LT(ms, 200.0) << "1M build exceeded 200ms budget";

    // Quick correctness: verify total boid count
    std::vector<int> h_cs(tc), h_ce(tc);
    cudaMemcpy(h_cs.data(), buf.d_cell_start, tc * sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_ce.data(), buf.d_cell_end, tc * sizeof(int), cudaMemcpyDeviceToHost);
    int total = 0;
    for (int cid = 0; cid < tc; cid++) {
        if (h_cs[cid] >= 0) total += h_ce[cid] - h_cs[cid];
    }
    ASSERT_EQ(total, N) << "1M build lost boids";

    gpu_buffers_free(buf);
}
