#include "ecs/world.h"
#include "ecs/systems.h"
#include "ecs/spawn.h"
#include "ecs/stats.h"
#include "render/renderer.h"
#include "components.h"
#include "render_state.h"
#include <flecs.h>
#include <raylib.h>
#include <iostream>

int main() {
    std::cout << "[DEBUG] Starting main..." << std::endl;
    
    // Initialize FLECS world
    flecs::world world;
    std::cout << "[DEBUG] World created" << std::endl;
    
    init_world(world);
    std::cout << "[DEBUG] World initialized" << std::endl;
    
    register_all_systems(world);
    std::cout << "[DEBUG] Systems registered" << std::endl;
    
    register_stats_system(world);
    std::cout << "[DEBUG] Stats system registered" << std::endl;

    // Spawn initial population
    spawn_initial_population(world);
    std::cout << "[DEBUG] Population spawned" << std::endl;

    // Initialize Raylib renderer
    const SimConfig& config = world.get<SimConfig>();
    std::cout << "[DEBUG] Got config" << std::endl;
    
    init_renderer(static_cast<int>(config.world_width),
                  static_cast<int>(config.world_height),
                  "COMP6216 Boid Swarm");
    std::cout << "[DEBUG] Renderer initialized" << std::endl;

    // Main loop
    int frame = 0;
    while (!WindowShouldClose() && frame < 100) {
        float dt = GetFrameTime();
        std::cout << "[DEBUG] Frame " << frame << ", dt=" << dt << std::endl;

        // Advance all FLECS systems
        world.progress(dt);
        std::cout << "[DEBUG] World progressed" << std::endl;

        // Get render state populated by RenderSyncSystem
        const RenderState& rs = world.get<RenderState>();
        std::cout << "[DEBUG] Got render state, boids=" << rs.boids.size() << std::endl;

        // Draw the frame
        render_frame(rs);
        std::cout << "[DEBUG] Frame rendered" << std::endl;
        
        frame++;
    }

    std::cout << "[DEBUG] Exiting main loop" << std::endl;

    // Cleanup
    close_renderer();
    std::cout << "[DEBUG] Cleanup done" << std::endl;

    return 0;
}
