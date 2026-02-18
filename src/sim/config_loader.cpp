#include "config_loader.h"
#include "components.h"

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

float parse_float(const std::string& val, int line_num) {
    try {
        return std::stof(val);
    } catch (...) {
        throw std::runtime_error(
            "config line " + std::to_string(line_num) +
            ": cannot parse '" + val + "' as float");
    }
}

int parse_int(const std::string& val, int line_num) {
    try {
        return std::stoi(val);
    } catch (...) {
        throw std::runtime_error(
            "config line " + std::to_string(line_num) +
            ": cannot parse '" + val + "' as int");
    }
}

bool apply_field(SimConfig& config, const std::string& key,
                 const std::string& val, int line_num) {
    // Infection probabilities
    if (key == "p_initial_infect_normal")  { config.p_initial_infect_normal = parse_float(val, line_num); }
    else if (key == "p_initial_infect_doctor")  { config.p_initial_infect_doctor = parse_float(val, line_num); }
    else if (key == "p_infect_normal")          { config.p_infect_normal = parse_float(val, line_num); }
    else if (key == "p_infect_doctor")          { config.p_infect_doctor = parse_float(val, line_num); }
    // Reproduction
    else if (key == "p_offspring_normal")       { config.p_offspring_normal = parse_float(val, line_num); }
    else if (key == "p_offspring_doctor")       { config.p_offspring_doctor = parse_float(val, line_num); }
    else if (key == "offspring_mean_normal")    { config.offspring_mean_normal = parse_float(val, line_num); }
    else if (key == "offspring_stddev_normal")  { config.offspring_stddev_normal = parse_float(val, line_num); }
    else if (key == "offspring_mean_doctor")    { config.offspring_mean_doctor = parse_float(val, line_num); }
    else if (key == "offspring_stddev_doctor")  { config.offspring_stddev_doctor = parse_float(val, line_num); }
    else if (key == "reproduction_cooldown")    { config.reproduction_cooldown = parse_float(val, line_num); }
    // Cure & transition
    else if (key == "p_cure")                   { config.p_cure = parse_float(val, line_num); }
    else if (key == "p_become_doctor")          { config.p_become_doctor = parse_float(val, line_num); }
    else if (key == "p_antivax")                { config.p_antivax = parse_float(val, line_num); }
    // Interaction radii
    else if (key == "r_interact_normal")        { config.r_interact_normal = parse_float(val, line_num); }
    else if (key == "r_interact_doctor")        { config.r_interact_doctor = parse_float(val, line_num); }
    // Time
    else if (key == "t_death")                  { config.t_death = parse_float(val, line_num); }
    else if (key == "t_adult")                  { config.t_adult = parse_float(val, line_num); }
    // World bounds
    else if (key == "world_width")              { config.world_width = parse_float(val, line_num); }
    else if (key == "world_height")             { config.world_height = parse_float(val, line_num); }
    // Population (int)
    else if (key == "initial_normal_count")     { config.initial_normal_count = parse_int(val, line_num); }
    else if (key == "initial_doctor_count")     { config.initial_doctor_count = parse_int(val, line_num); }
    // Movement
    else if (key == "max_speed")                { config.max_speed = parse_float(val, line_num); }
    else if (key == "max_force")                { config.max_force = parse_float(val, line_num); }
    else if (key == "min_speed")                { config.min_speed = parse_float(val, line_num); }
    else if (key == "separation_weight")        { config.separation_weight = parse_float(val, line_num); }
    else if (key == "alignment_weight")         { config.alignment_weight = parse_float(val, line_num); }
    else if (key == "cohesion_weight")          { config.cohesion_weight = parse_float(val, line_num); }
    else if (key == "separation_radius")        { config.separation_radius = parse_float(val, line_num); }
    else if (key == "alignment_radius")         { config.alignment_radius = parse_float(val, line_num); }
    else if (key == "cohesion_radius")          { config.cohesion_radius = parse_float(val, line_num); }
    // Debuffs
    else if (key == "debuff_p_cure_infected")              { config.debuff_p_cure_infected = parse_float(val, line_num); }
    else if (key == "debuff_r_interact_doctor_infected")   { config.debuff_r_interact_doctor_infected = parse_float(val, line_num); }
    else if (key == "debuff_p_offspring_doctor_infected")   { config.debuff_p_offspring_doctor_infected = parse_float(val, line_num); }
    else if (key == "debuff_r_interact_normal_infected")   { config.debuff_r_interact_normal_infected = parse_float(val, line_num); }
    else if (key == "debuff_p_offspring_normal_infected")   { config.debuff_p_offspring_normal_infected = parse_float(val, line_num); }
    // Antivax
    else if (key == "antivax_repulsion_radius")  { config.antivax_repulsion_radius = parse_float(val, line_num); }
    else if (key == "antivax_repulsion_weight")  { config.antivax_repulsion_weight = parse_float(val, line_num); }
    else {
        std::cerr << "config warning: unknown key '" << key << "' on line "
                  << line_num << " (ignored)\n";
        return false;
    }
    return true;
}

} // anonymous namespace

bool load_config(const std::string& path, SimConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        // Skip section headers
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            continue;
        }

        // Parse key = value
        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            throw std::runtime_error(
                "config line " + std::to_string(line_num) +
                ": expected 'key = value', got '" + trimmed + "'");
        }

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val = trim(trimmed.substr(eq_pos + 1));

        if (key.empty() || val.empty()) {
            throw std::runtime_error(
                "config line " + std::to_string(line_num) +
                ": empty key or value");
        }

        apply_field(config, key, val, line_num);
    }

    return true;
}
