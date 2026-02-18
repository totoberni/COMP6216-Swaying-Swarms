#include <gtest/gtest.h>
#include <flecs.h>
#include "components.h"
#include "spatial_grid.h"
#include "ecs/systems.h"
#include "ecs/stats.h"
#include "ecs/spawn.h"

// Helper: register all component types needed for antivax tests
static void register_components(flecs::world& world) {
    world.component<Position>();
    world.component<Velocity>();
    world.component<Heading>();
    world.component<Health>();
    world.component<InfectionState>();
    world.component<ReproductionCooldown>();
    world.component<NormalBoid>();
    world.component<DoctorBoid>();
    world.component<AntivaxBoid>();
    world.component<Male>();
    world.component<Female>();
    world.component<Infected>();
    world.component<Alive>();
    world.component<SpatialGrid>();
}

// Helper: set required singletons with a base config
static void set_singletons(flecs::world& world, SimConfig config = SimConfig{}) {
    world.set<SimConfig>(config);
    world.set<SimStats>({});
    SpatialGrid grid(1920.0f, 1080.0f, 40.0f);
    world.set<SpatialGrid>(std::move(grid));
}

// ============================================================
// B2: Antivax Spawn — Mutually Exclusive Tags
// ============================================================

TEST(AntivaxSpawn, MutuallyExclusiveTags) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_antivax = 1.0f;          // All spawned normals become antivax
    config.p_initial_infect_normal = 0.0f;  // No initial infection to keep things simple
    set_singletons(world, config);

    spawn_normal_boids(world, 50);

    // Verify: no entity has BOTH NormalBoid AND AntivaxBoid
    auto q_both = world.query<const Position>();
    bool found_dual_tag = false;
    int total_antivax = 0;
    int total_normal = 0;
    q_both.each([&](flecs::entity e, const Position&) {
        bool is_normal = e.has<NormalBoid>();
        bool is_antivax = e.has<AntivaxBoid>();
        if (is_normal && is_antivax) {
            found_dual_tag = true;
        }
        if (is_antivax) total_antivax++;
        if (is_normal) total_normal++;
    });

    EXPECT_FALSE(found_dual_tag)
        << "No entity should have both NormalBoid AND AntivaxBoid tags";
    EXPECT_EQ(total_antivax, 50)
        << "All 50 spawned boids should be AntivaxBoid when p_antivax = 1.0";
    EXPECT_EQ(total_normal, 0)
        << "Zero entities should have NormalBoid when p_antivax = 1.0";
}

TEST(AntivaxSpawn, ZeroProbability) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_antivax = 0.0f;          // No antivax
    config.p_initial_infect_normal = 0.0f;
    set_singletons(world, config);

    spawn_normal_boids(world, 50);

    auto q_all = world.query<const Position>();
    int total_normal = 0;
    int total_antivax = 0;
    q_all.each([&](flecs::entity e, const Position&) {
        if (e.has<NormalBoid>()) total_normal++;
        if (e.has<AntivaxBoid>()) total_antivax++;
    });

    EXPECT_EQ(total_normal, 50)
        << "All 50 spawned boids should be NormalBoid when p_antivax = 0.0";
    EXPECT_EQ(total_antivax, 0)
        << "Zero entities should have AntivaxBoid when p_antivax = 0.0";
}

// ============================================================
// B3: Antivax Infection — Same-Swarm Only
// ============================================================

// Infected antivax spreads to adjacent antivax boid
TEST(AntivaxInfection, SameSwarmOnly) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_infect_normal = 1.0f;    // Deterministic infection
    // r_interact_normal = 30.0f (default). Effective radius = 30 * 0.8 = 24 px.
    // Place boids 10 px apart — well within 24px
    set_singletons(world, config);

    // Infected AntivaxBoid at (500, 500)
    auto infected = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Infected>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // Healthy AntivaxBoid at (510, 500) — 10 px away
    auto healthy = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{510.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_infection_system(world);

    for (int i = 0; i < 60; ++i) {
        world.progress(1.0f / 60.0f);
    }

    EXPECT_TRUE(healthy.has<Infected>())
        << "Adjacent AntivaxBoid should become infected (same-swarm infection)";
}

// Infected antivax must NOT infect a NormalBoid neighbor
TEST(AntivaxInfection, NoCrossSwarm) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_infect_normal = 1.0f;
    set_singletons(world, config);

    // Infected AntivaxBoid at (500, 500)
    auto infected = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Infected>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // Healthy NormalBoid at (510, 500) — 10 px away
    auto normal = world.entity()
        .add<NormalBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{510.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_infection_system(world);

    for (int i = 0; i < 60; ++i) {
        world.progress(1.0f / 60.0f);
    }

    EXPECT_FALSE(normal.has<Infected>())
        << "NormalBoid should NOT be infected by an AntivaxBoid (cross-swarm prevention)";
}

// ============================================================
// B4: Antivax Reproduction — Offspring Inherit Tag
// ============================================================

TEST(AntivaxReproduction, OffspringInherit) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_offspring_normal = 1.0f;     // Deterministic reproduction
    config.offspring_mean_normal = 3.0f;  // 3 offspring per event
    config.offspring_stddev_normal = 0.0f;
    config.reproduction_cooldown = 60.0f; // Long cooldown prevents exponential growth
    config.p_initial_infect_normal = 0.0f;
    config.p_antivax = 0.0f;
    // r_interact_normal = 30.0f — place parents within that range
    set_singletons(world, config);

    // Male AntivaxBoid at (500, 500)
    auto male = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // Female AntivaxBoid at (510, 500) — 10 px away, within r_interact_normal
    auto female = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{510.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_reproduction_system(world);

    // 5 frames is enough for one reproduction event; long cooldown prevents further
    for (int i = 0; i < 5; ++i) {
        world.progress(1.0f / 60.0f);
    }

    // Count all AntivaxBoid and NormalBoid entities
    int antivax_count = 0;
    int normal_count = 0;
    auto q = world.query<const Position>();
    q.each([&](flecs::entity e, const Position&) {
        if (e.has<AntivaxBoid>()) antivax_count++;
        if (e.has<NormalBoid>()) normal_count++;
    });

    EXPECT_GT(antivax_count, 2)
        << "AntivaxBoid offspring should have been born (count should exceed 2 parents)";
    EXPECT_EQ(normal_count, 0)
        << "No NormalBoid entities should exist — offspring must inherit AntivaxBoid tag";
}

TEST(AntivaxReproduction, NoCrossSwarm) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_offspring_normal = 1.0f;
    config.offspring_mean_normal = 3.0f;
    config.offspring_stddev_normal = 0.0f;
    config.reproduction_cooldown = 60.0f; // Long cooldown prevents exponential growth
    config.p_initial_infect_normal = 0.0f;
    config.p_antivax = 0.0f;
    set_singletons(world, config);

    // AntivaxBoid male at (500, 500)
    auto antivax_male = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // NormalBoid female at (510, 500) — 10 px away, opposite sex
    auto normal_female = world.entity()
        .add<NormalBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{510.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_reproduction_system(world);

    for (int i = 0; i < 5; ++i) {
        world.progress(1.0f / 60.0f);
    }

    // Count all entities with Position
    int total_count = 0;
    auto q = world.query<const Position>();
    q.each([&](flecs::entity e, const Position&) {
        total_count++;
    });

    EXPECT_EQ(total_count, 2)
        << "No offspring should be produced between AntivaxBoid and NormalBoid (cross-swarm prevented)";
}

// ============================================================
// B5: Doctor Cures Antivax
// ============================================================

TEST(DoctorCuresAntivax, AntivaxGetsCured) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.p_cure = 1.0f;  // Deterministic cure
    // r_interact_doctor = 40.0f — place doctor within that range of the antivax boid
    set_singletons(world, config);

    // Infected AntivaxBoid at (500, 500)
    auto antivax = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Infected>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // Healthy DoctorBoid at (510, 510) — ~14 px away, within r_interact_doctor (40)
    auto doctor = world.entity()
        .add<DoctorBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{510.0f, 510.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_cure_system(world);

    for (int i = 0; i < 60; ++i) {
        world.progress(1.0f / 60.0f);
    }

    EXPECT_FALSE(antivax.has<Infected>())
        << "AntivaxBoid should have been cured by the adjacent DoctorBoid";
}

// ============================================================
// B6: Antivax Death Tracking
// ============================================================

TEST(AntivaxDeath, StatsTracked) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    config.t_death = 0.1f;  // Fast death (0.1 seconds)
    set_singletons(world, config);

    // Infected AntivaxBoid that will die quickly
    auto dying = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Infected>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, config.t_death})
        .set(ReproductionCooldown{0.0f});

    register_aging_system(world);
    register_death_system(world);

    // Run 60 frames at 1/60 dt = 1 second, well past 0.1s t_death
    for (int i = 0; i < 60; ++i) {
        world.progress(1.0f / 60.0f);
    }

    EXPECT_FALSE(dying.has<Alive>())
        << "AntivaxBoid infected past t_death should no longer have Alive tag";

    const SimStats& stats = world.get<SimStats>();
    EXPECT_GE(stats.dead_antivax, 1)
        << "dead_antivax counter should be at least 1";
    EXPECT_GE(stats.dead_total, 1)
        << "dead_total counter should be at least 1";
}

// ============================================================
// B7: Antivax Stats Counting
// ============================================================

TEST(AntivaxStats, AliveCount) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    set_singletons(world, config);

    // Spawn 5 alive AntivaxBoid entities
    for (int i = 0; i < 5; ++i) {
        world.entity()
            .add<AntivaxBoid>()
            .add<Alive>()
            .add<Male>()
            .set(Position{static_cast<float>(100 + i * 50), 500.0f})
            .set(Velocity{0.0f, 0.0f})
            .set(Heading{0.0f})
            .set(Health{0.0f, 60.0f})
            .set(InfectionState{0.0f, 5.0f})
            .set(ReproductionCooldown{0.0f});
    }

    register_stats_system(world);

    world.progress(1.0f / 60.0f);

    const SimStats& stats = world.get<SimStats>();
    EXPECT_EQ(stats.antivax_alive, 5)
        << "antivax_alive should be 5 after one frame with 5 alive AntivaxBoid entities";
}

// ============================================================
// B8: Antivax Doctor-Avoidance Steering
// ============================================================

TEST(AntivaxSteering, FleesDoctor) {
    flecs::world world;
    register_components(world);

    SimConfig config{};
    // antivax_repulsion_radius = 100.0f (default)
    // antivax_repulsion_weight = 3.0f (default)
    // AntivaxBoid at (500, 500), DoctorBoid at (550, 500) — 50 px apart, within radius
    set_singletons(world, config);

    // AntivaxBoid at (500, 500) with zero velocity
    auto antivax = world.entity()
        .add<AntivaxBoid>()
        .add<Alive>()
        .add<Male>()
        .set(Position{500.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    // DoctorBoid at (550, 500) — 50 px to the right, within repulsion radius
    auto doctor = world.entity()
        .add<DoctorBoid>()
        .add<Alive>()
        .add<Female>()
        .set(Position{550.0f, 500.0f})
        .set(Velocity{0.0f, 0.0f})
        .set(Heading{0.0f})
        .set(Health{0.0f, 60.0f})
        .set(InfectionState{0.0f, 5.0f})
        .set(ReproductionCooldown{0.0f});

    register_rebuild_grid_system(world);
    register_antivax_steering_system(world);
    register_movement_system(world);

    for (int i = 0; i < 60; ++i) {
        world.progress(1.0f / 60.0f);
    }

    // The AntivaxBoid should be fleeing leftward (vx < 0) away from the DoctorBoid on the right
    const Velocity& vel = antivax.get<Velocity>();
    EXPECT_LT(vel.vx, 0.0f)
        << "AntivaxBoid velocity.vx should be negative (fleeing leftward away from DoctorBoid on the right)";
}
