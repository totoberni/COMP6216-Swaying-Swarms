#include "ecs/world.h"
#include "ecs/systems.h"
#include "ecs/spawn.h"
#include "ecs/stats.h"
#include "render/renderer.h"
#include "components.h"
#include "render_state.h"
#include <flecs.h>
#include <raylib.h>

int main() {
    // Initialize FLECS world
    flecs::world world;
    init_world(world);
    register_all_systems(world);
    register_stats_system(world);

    // Spawn initial population
    spawn_initial_population(world);

    // Initialize Raylib renderer
    const SimConfig& config = world.get<SimConfig>();
    init_renderer(static_cast<int>(config.world_width),
                  static_cast<int>(config.world_height),
                  "COMP6216 Boid Swarm");

    // Main loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Advance all FLECS systems
        world.progress(dt);

        // Get render state populated by RenderSyncSystem
        const RenderState& rs = world.get<RenderState>();

        // Draw the frame
        render_frame(rs);
    }

    // Cleanup
    close_renderer();
    return 0;
}
