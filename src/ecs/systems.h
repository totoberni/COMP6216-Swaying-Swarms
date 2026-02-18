#pragma once

#include <flecs.h>

void register_all_systems(flecs::world& world);

// Individual system registration functions (also available for unit tests)
void register_rebuild_grid_system(flecs::world& world);
void register_cure_system(flecs::world& world);
