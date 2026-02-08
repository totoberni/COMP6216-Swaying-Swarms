#pragma once

#include <flecs.h>
#include <string>

void init_world(flecs::world& world, const std::string& config_path = "config.ini");
