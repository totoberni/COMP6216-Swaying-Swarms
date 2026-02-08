#pragma once

#include <string>

struct SimConfig;

// Load simulation parameters from an INI-style config file.
// Returns true if the file was found and parsed.
// Returns false if the file does not exist (config unchanged, defaults kept).
// Throws std::runtime_error on parse errors (malformed lines).
bool load_config(const std::string& path, SimConfig& config);
