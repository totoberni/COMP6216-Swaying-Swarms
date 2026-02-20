#include <gtest/gtest.h>
#include <flecs.h>
#include "components.h"
#include "spatial_grid.h"
#include "ecs/systems.h"

// Helper: register all component types needed for cure/grid systems
static void register_components(flecs::world& world) {
    world.component<Position>();
    world.component<Velocity>();
    world.component<Heading>();
    world.component<InfectionState>();
    world.component<NormalBoid>();
    world.component<DoctorBoid>();
    world.component<Infected>();
    world.component<SpatialGrid>();
}

// Helper: set required singletons with deterministic p_cure = 1.0
static void set_singletons(flecs::world& world) {
    SimConfig config{};
    config.p_cure = 1.0f;
    world.set<SimConfig>(config);

    world.set<SimStats>({});

    SpatialGrid grid(1920.0f, 1080.0f, 40.0f);
    world.set<SpatialGrid>(std::move(grid));
}

// Test 1: A doctor that is infected cannot cure itself.
// The cure system must skip nid == e.id(), so the lone infected doctor
// should still carry the Infected tag after 120 iterations.
TEST(CureContract, DoctorCannotCureSelf) {
    flecs::world world;

    register_components(world);
    set_singletons(world);

    // Create a single infected doctor with a long time-to-death (far from dying)
    auto doctor = world.entity()
        .add<DoctorBoid>()
        .add<Infected>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(InfectionState{0.0f, 5.0f});

    register_rebuild_grid_system(world);
    register_cure_system(world);

    // Run 120 ticks (= 2 s at 60 fps).  t_death = 5 s so the doctor stays alive.
    for (int i = 0; i < 120; ++i) {
        world.progress(1.0f / 60.0f);
    }

    // The doctor must still be infected — it cannot have cured itself
    EXPECT_TRUE(doctor.has<Infected>())
        << "Doctor should not be able to cure itself (no-self-cure contract)";
}

// Test 2: A healthy doctor that is adjacent to an infected doctor should cure it.
// With p_cure = 1.0 and both doctors within r_interact_doctor (40 px), the
// infected doctor must lose its Infected tag within 120 iterations.
TEST(CureContract, DoctorCanCureOtherDoctor) {
    flecs::world world;

    register_components(world);
    set_singletons(world);

    // Doctor 1 — infected, at (500, 500)
    auto doctor1 = world.entity()
        .add<DoctorBoid>()
        .add<Infected>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(InfectionState{0.0f, 5.0f});

    // Doctor 2 — healthy, at (510, 510) — 14 px away, within r_interact_doctor (40)
    auto doctor2 = world.entity()
        .add<DoctorBoid>()
        .set(Position{510.0f, 510.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(InfectionState{0.0f, 5.0f});

    register_rebuild_grid_system(world);
    register_cure_system(world);

    // Run 120 ticks.  With p_cure = 1.0 this should succeed on the very first
    // tick that the grid is populated, so 120 ticks is very conservative.
    for (int i = 0; i < 120; ++i) {
        world.progress(1.0f / 60.0f);
    }

    // Doctor 1 must have been cured by Doctor 2
    EXPECT_FALSE(doctor1.has<Infected>())
        << "Infected doctor should have been cured by the adjacent healthy doctor";

    // Doctor 2 (the healthy curer) should remain uninfected
    EXPECT_FALSE(doctor2.has<Infected>())
        << "The healthy curing doctor should not have become infected";
}
