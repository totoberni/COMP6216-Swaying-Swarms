#pragma once

#include <flecs.h>

void register_all_systems(flecs::world& world);

// Individual system registration functions (also available for unit tests)
void register_rebuild_grid_system(flecs::world& world);
void register_steering_system(flecs::world& world);
void register_movement_system(flecs::world& world);
void register_collision_system(flecs::world& world);
void register_infection_system(flecs::world& world);
void register_cure_system(flecs::world& world);
void register_death_recovery_system(flecs::world& world);
void register_immunity_decay_system(flecs::world& world);
void register_render_sync_system(flecs::world& world);
