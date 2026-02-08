#include "world.h"
#include "components.h"
#include "config_loader.h"
#include "spatial_grid.h"
#include "render_state.h"
#include <flecs.h>
#include <algorithm>
#include <iostream>

void init_world(flecs::world& world, const std::string& config_path) {
    // Register all components from components.h
    world.component<Position>();
    world.component<Velocity>();
    world.component<Heading>();
    world.component<Health>();
    world.component<InfectionState>();
    world.component<ReproductionCooldown>();

    // Register tag components
    world.component<NormalBoid>();
    world.component<DoctorBoid>();
    world.component<Male>();
    world.component<Female>();
    world.component<Infected>();
    world.component<Alive>();
    world.component<Antivax>();

    // Register SpatialGrid as a component (required before using as singleton)
    world.component<SpatialGrid>();
    
    // Load SimConfig from file (or use defaults if file not found)
    SimConfig config{};
    if (load_config(config_path, config)) {
        std::cout << "Loaded config from " << config_path << "\n";
    }
    world.set<SimConfig>(config);

    // Set SimStats singleton (zeroed)
    world.set<SimStats>({});

    // Set SimulationState singleton (not paused)
    world.set<SimulationState>({});

    // Set RenderState singleton (empty)
    world.set<RenderState>({});

    // Create SpatialGrid singleton — cell size matches largest interaction radius
    float cell_size = std::max(config.r_interact_normal, config.r_interact_doctor);
    SpatialGrid grid(config.world_width, config.world_height, cell_size);
    world.set<SpatialGrid>(std::move(grid));
}
