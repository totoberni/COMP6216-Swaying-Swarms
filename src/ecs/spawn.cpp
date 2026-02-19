#include "spawn.h"
#include "components.h"
#include "spatial_grid.h"
#include <flecs.h>
#include <random>
#include <cmath>
#include <algorithm>

namespace {
    // Seeded random engine for reproducible spawning
    std::mt19937 rng(42);
}

void spawn_normal_boids(flecs::world& world, int count) {
    const SimConfig& config = world.get<SimConfig>();

    std::uniform_real_distribution<float> dist_x(0.0f, config.world_width);
    std::uniform_real_distribution<float> dist_y(0.0f, config.world_height);
    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_infect(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_antivax(0.0f, 1.0f);

    for (int i = 0; i < count; ++i) {
        float x = dist_x(rng);
        float y = dist_y(rng);
        float angle = dist_angle(rng);
        float speed = config.max_speed * 1.0f; // Start with moderate speed

        auto boid = world.entity()
            .add<Alive>()
            .set(Position{x, y})
            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
            .set(Heading{angle})
            .set(Health{0.0f, 60.0f}) // age=0, lifespan=60s
            .set(ReproductionCooldown{0.0f});

        // Assign sex
        if (dist_sex(rng) < 0.5f) {
            boid.add<Male>();
        } else {
            boid.add<Female>();
        }

        // Assign swarm tag — mutually exclusive
        if (dist_antivax(rng) < config.p_antivax) {
            boid.add<AntivaxBoid>();
        } else {
            boid.add<NormalBoid>();
        }

        // Initial infection
        if (dist_infect(rng) < config.p_initial_infect_normal) {
            boid.add<Infected>();
            boid.set(InfectionState{0.0f, config.t_death});
        }
    }
}

void spawn_doctor_boids(flecs::world& world, int count) {
    const SimConfig& config = world.get<SimConfig>();

    std::uniform_real_distribution<float> dist_x(0.0f, config.world_width);
    std::uniform_real_distribution<float> dist_y(0.0f, config.world_height);
    std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> dist_sex(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_infect(0.0f, 1.0f);

    for (int i = 0; i < count; ++i) {
        float x = dist_x(rng);
        float y = dist_y(rng);
        float angle = dist_angle(rng);
        float speed = config.max_speed * 1.0f; // Start with moderate speed

        auto boid = world.entity()
            .add<DoctorBoid>()
            .add<Alive>()
            .set(Position{x, y})
            .set(Velocity{speed * std::cos(angle), speed * std::sin(angle)})
            .set(Heading{angle})
            .set(Health{0.0f, 60.0f}) // age=0, lifespan=60s
            .set(ReproductionCooldown{0.0f});

        // Assign sex
        if (dist_sex(rng) < 0.5f) {
            boid.add<Male>();
        } else {
            boid.add<Female>();
        }

        // Initial infection
        if (dist_infect(rng) < config.p_initial_infect_doctor) {
            boid.add<Infected>();
            boid.set(InfectionState{0.0f, config.t_death});
        }
    }
}

void spawn_initial_population(flecs::world& world) {
    const SimConfig& config = world.get<SimConfig>();

    spawn_normal_boids(world, config.initial_normal_count);
    spawn_doctor_boids(world, config.initial_doctor_count);
}

void reset_simulation(flecs::world& world) {
    // Delete all boid entities (both alive and dead)
    // Use Position component as marker since all boids have it
    world.defer_begin();

    auto query = world.query_builder<>()
        .with<Position>()
        .build();

    query.each([](flecs::entity e) {
        e.destruct();
    });

    world.defer_end();

    // Reset statistics and population history
    SimStats& stats = world.get_mut<SimStats>();
    stats.normal_alive = 0;
    stats.doctor_alive = 0;
    stats.dead_total = 0;
    stats.dead_normal = 0;
    stats.dead_doctor = 0;
    stats.newborns_total = 0;
    stats.newborns_normal = 0;
    stats.newborns_doctor = 0;
    stats.antivax_alive = 0;
    stats.dead_antivax = 0;
    stats.newborns_antivax = 0;
    stats.history_index = 0;
    stats.history_count = 0;
    for (int i = 0; i < SimStats::HISTORY_SIZE; ++i) {
        stats.history[i] = {};
    }

    // Rebuild spatial grid with current config (sliders may have changed radii).
    // Cell size = largest infection radius; steering uses dynamic search window expansion.
    const SimConfig& config = world.get<SimConfig>();
    float cell_size = std::max(config.r_interact_normal, config.r_interact_doctor);
    SpatialGrid new_grid(config.world_width, config.world_height, cell_size);
    world.set<SpatialGrid>(std::move(new_grid));

    // Re-spawn initial population
    spawn_initial_population(world);
}
