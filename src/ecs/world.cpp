#include "world.h"
#include "components.h"
#include "spatial_grid.h"
#include <flecs.h>

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

    // Set SimConfig singleton with default values
    world.set<SimConfig>({});

    // Set SimStats singleton (zeroed)
    world.set<SimStats>({});

    // Create SpatialGrid singleton
    const SimConfig& config = world.get<SimConfig>();
    world.set<SpatialGrid>(
        SpatialGrid(config.world_width, config.world_height, 50.0f)
    );
}
