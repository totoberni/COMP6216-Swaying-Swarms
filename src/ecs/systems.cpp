#include "systems.h"
#include <flecs.h>

// ============================================================
// System Registration Coordinator
// Individual systems defined in:
//   systems_steering.cpp     — grid rebuild, steering, movement
//   systems_infection.cpp    — collision, infection, cure
//   systems_lifecycle.cpp    — aging, death, doctor promotion
//   systems_reproduction.cpp — reproduction
//   systems_render_sync.cpp  — render state sync
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
    register_death_recovery_system(world);
    register_immunity_decay_system(world);

    // OnStore
    register_render_sync_system(world);
}
