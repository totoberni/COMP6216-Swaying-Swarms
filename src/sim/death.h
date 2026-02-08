#pragma once

// Pure C++ death logic — no FLECS includes

// Check if infected entity should die based on infection time
bool should_die(float time_infected, float t_death);
