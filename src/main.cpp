#include "ecs/world.h"
#include "ecs/systems.h"
#include "ecs/spawn.h"
#include "ecs/stats.h"
#include "render/renderer.h"
#include "components.h"
#include "render_state.h"
#include <flecs.h>
#include <raylib.h>
#include <string>

int main(int argc, char* argv[]) {
    std::string config_path = (argc > 1) ? argv[1] : "config.ini";

    // Initialize FLECS world
    flecs::world world;
    init_world(world, config_path);
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

        // Check for reset request
        SimulationState& sim_state = world.get_mut<SimulationState>();

        // Keyboard shortcuts
        if (IsKeyPressed(KEY_SPACE)) {
            sim_state.is_paused = !sim_state.is_paused;
        }
        if (IsKeyPressed(KEY_R)) {
            sim_state.reset_requested = true;
        }

        if (IsKeyPressed(KEY_H)) {
            sim_state.show_stats_overlay = !sim_state.show_stats_overlay;
        }

        if (sim_state.reset_requested) {
            reset_simulation(world);
            sim_state.reset_requested = false;
            sim_state.is_paused = false;  // Unpause after reset
        }

        // Advance all FLECS systems only if not paused
        if (!sim_state.is_paused) {
            world.progress(dt);
        }

        // Get render state populated by RenderSyncSystem
        const RenderState& rs = world.get<RenderState>();

        // Draw the frame
        render_frame(rs);
    }

    // Cleanup
    close_renderer();

    return 0;
}
