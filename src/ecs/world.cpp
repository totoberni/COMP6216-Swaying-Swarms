#include "world.h"
#include "components.h"
#include "spatial_grid.h"
#include "render_state.h"
#include <flecs.h>
#include <algorithm>
#include <iostream>

void init_world(flecs::world& world) {
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
    
    // Set SimConfig singleton with default values
    world.set<SimConfig>({});

    // Set SimStats singleton (zeroed)
    world.set<SimStats>({});

    // Set RenderState singleton (empty)
    world.set<RenderState>({});

    // Create SpatialGrid singleton — cell size matches largest interaction radius
    const SimConfig& config = world.get<SimConfig>();
    float cell_size = std::max(config.r_interact_normal, config.r_interact_doctor);
    
    std::cout << "[WORLD] Creating SpatialGrid..." << std::endl;
    SpatialGrid grid(config.world_width, config.world_height, cell_size);
    std::cout << "[WORLD] SpatialGrid created, setting as singleton..." << std::endl;
    world.set<SpatialGrid>(std::move(grid));
    std::cout << "[WORLD] SpatialGrid singleton set!" << std::endl;
}
