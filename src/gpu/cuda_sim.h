#pragma once

#include <string>

// Forward declarations — avoid pulling flecs.h and components.h into every TU
namespace flecs { struct world; }
struct SimConfig;

// GPU headless simulation: extracts FLECS entities, runs full sim on GPU, writes CSV + summary
void gpu_run_headless(flecs::world& world, const SimConfig& config,
                      const std::string& config_path);
