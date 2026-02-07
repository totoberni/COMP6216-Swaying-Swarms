#include "systems.h"
#include "components.h"
#include "spatial_grid.h"
#include "render_state.h"
#include <flecs.h>
#include <cmath>

// ============================================================
// PreUpdate Phase: Spatial Grid Rebuild
// ============================================================

void register_rebuild_grid_system(flecs::world& world) {
    world.system("RebuildGridSystem")
        .kind(flecs::PreUpdate)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            SpatialGrid& grid = w.get_mut<SpatialGrid>();

            // Clear grid once per frame
            grid.clear();

            // Insert all entities with Position using a query
            auto q = w.query<const Position>();
            q.each([&grid](flecs::entity e, const Position& pos) {
                grid.insert(e.id(), pos.x, pos.y);
            });
        });
}

// ============================================================
// OnUpdate Phase: Steering and Movement
// ============================================================

void register_steering_system(flecs::world& world) {
    world.system<const Position, Velocity, const Alive>("SteeringSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, const Position& pos, Velocity& vel, const Alive&) {
            // Stub: Will implement separation, alignment, cohesion
            // Query neighbors from spatial grid
            // Apply steering forces with max_force and max_speed limits
        });
}

void register_movement_system(flecs::world& world) {
    world.system<Position, const Velocity>("MovementSystem")
        .kind(flecs::OnUpdate)
        .each([](flecs::iter& it, size_t index, Position& pos, const Velocity& vel) {
            const SimConfig& config = it.world().get<SimConfig>();
            float dt = it.delta_time();

            // Apply velocity to position
            pos.x += vel.vx * dt;
            pos.y += vel.vy * dt;

            // Wrap around world bounds
            if (pos.x < 0.0f) pos.x += config.world_width;
            if (pos.x >= config.world_width) pos.x -= config.world_width;
            if (pos.y < 0.0f) pos.y += config.world_height;
            if (pos.y >= config.world_height) pos.y -= config.world_height;

            // Update heading based on velocity
            flecs::entity e = it.entity(index);
            if (e.has<Heading>()) {
                Heading& heading = e.get_mut<Heading>();
                heading.angle = std::atan2(vel.vy, vel.vx);
            }
        });
}

// ============================================================
// PostUpdate Phase: Collisions and Behavior
// ============================================================

void register_collision_system(flecs::world& world) {
    world.system<const Position, const Alive>("CollisionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Position& pos, const Alive&) {
            // Stub: Detect collisions via spatial grid
            // Will trigger infection, cure, and reproduction checks
        });
}

void register_infection_system(flecs::world& world) {
    world.system<InfectionState, const Alive>("InfectionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, InfectionState& infection, const Alive&) {
            // Stub: Update infection timers, trigger death
        });
}

void register_cure_system(flecs::world& world) {
    world.system<const Alive>("CureSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Alive&) {
            // Stub: Doctor cures infected boids
        });
}

void register_reproduction_system(flecs::world& world) {
    world.system<const Position, ReproductionCooldown, const Alive>("ReproductionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::iter& it, size_t index, const Position& pos, ReproductionCooldown& cooldown, const Alive&) {
            // Stub: Spawn offspring when conditions met
            float dt = it.delta_time();
            if (cooldown.cooldown > 0.0f) {
                cooldown.cooldown -= dt;
            }
        });
}

void register_death_system(flecs::world& world) {
    world.system<const Health, const InfectionState, const Alive>("DeathSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Health& health, const InfectionState& infection, const Alive&) {
            // Stub: Remove Alive tag when death conditions met
        });
}

void register_doctor_promotion_system(flecs::world& world) {
    world.system<const Alive>("DoctorPromotionSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const Alive&) {
            // Stub: Promote adult normal boids to doctors
        });
}

// ============================================================
// OnStore Phase: Render Sync
// ============================================================

void register_render_sync_system(flecs::world& world) {
    world.system("RenderSyncSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            // Stub: Populate RenderState from FLECS queries
            // Query all alive boids and build BoidRenderData vector
        });
}

// ============================================================
// System Registration
// ============================================================

void register_all_systems(flecs::world& world) {
    // PreUpdate
    register_rebuild_grid_system(world);

    // OnUpdate
    register_steering_system(world);
    register_movement_system(world);

    // PostUpdate
    register_collision_system(world);
    register_infection_system(world);
    register_cure_system(world);
    register_reproduction_system(world);
    register_death_system(world);
    register_doctor_promotion_system(world);

    // OnStore
    register_render_sync_system(world);
}
