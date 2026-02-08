#pragma once

// Pure C++ aging logic — no FLECS includes

// Increment entity age by delta time
void age_entity(float& age, float dt);

// Increment infection timer by delta time
void tick_infection(float& time_infected, float dt);
