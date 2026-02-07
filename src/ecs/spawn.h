#pragma once

#include <flecs.h>

void spawn_normal_boids(flecs::world& world, int count);
void spawn_doctor_boids(flecs::world& world, int count);
void spawn_initial_population(flecs::world& world);
