#include "renderer.h"
#include "render_config.h"
#include <raylib.h>
#include <random>
#include <cmath>

// ============================================================
// Standalone demo for the renderer
// ============================================================

struct DemoBoid {
    float x, y;
    float vx, vy;
    float angle;
    uint32_t color;
    float radius;
    bool is_doctor;
};

int main() {
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    const int NUM_BOIDS = 200;

    init_renderer(WIDTH, HEIGHT, "Boid Renderer Demo");

    // Initialize random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> x_dist(0.0f, static_cast<float>(WIDTH));
    std::uniform_real_distribution<float> y_dist(0.0f, static_cast<float>(HEIGHT));
    std::uniform_real_distribution<float> vel_dist(-2.0f, 2.0f);

    // Create random boids
    std::vector<DemoBoid> boids;
    for (int i = 0; i < NUM_BOIDS; ++i) {
        DemoBoid boid;
        boid.x = x_dist(gen);
        boid.y = y_dist(gen);
        boid.vx = vel_dist(gen);
        boid.vy = vel_dist(gen);
        boid.angle = std::atan2(boid.vy, boid.vx);

        // 10% doctors, 90% normal
        boid.is_doctor = (i < NUM_BOIDS / 10);
        boid.color = boid.is_doctor ? RenderConfig::COLOR_DOCTOR : RenderConfig::COLOR_NORMAL;
        boid.radius = boid.is_doctor ? 40.0f : 30.0f;

        boids.push_back(boid);
    }

    // Dummy stats
    SimStats stats;
    stats.normal_alive = 180;
    stats.doctor_alive = 20;
    stats.dead_total = 50;
    stats.dead_normal = 45;
    stats.dead_doctor = 5;
    stats.newborns_total = 30;
    stats.newborns_normal = 25;
    stats.newborns_doctor = 5;

    // Main loop
    while (!WindowShouldClose()) {
        // Update boid positions with wraparound
        for (auto& boid : boids) {
            boid.x += boid.vx;
            boid.y += boid.vy;

            // Wraparound
            if (boid.x < 0) boid.x += WIDTH;
            if (boid.x >= WIDTH) boid.x -= WIDTH;
            if (boid.y < 0) boid.y += HEIGHT;
            if (boid.y >= HEIGHT) boid.y -= HEIGHT;

            // Update angle to match velocity
            boid.angle = std::atan2(boid.vy, boid.vx);
        }

        // Prepare render state
        RenderState render_state;
        render_state.stats = stats;

        for (const auto& boid : boids) {
            BoidRenderData render_boid;
            render_boid.x = boid.x;
            render_boid.y = boid.y;
            render_boid.angle = boid.angle;
            render_boid.color = boid.color;
            render_boid.radius = boid.radius;
            render_boid.is_doctor = boid.is_doctor;
            render_state.boids.push_back(render_boid);
        }

        // Render
        render_frame(render_state);
    }

    close_renderer();
    return 0;
}
