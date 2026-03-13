#include "config_loader.h"
#include "components.h"

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <cstring>

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

bool apply_swarm_field(SwarmParams& sp, const std::string& key,
                       const std::string& val, int line_num) {
    if      (key == "max_speed")          sp.max_speed = parse_float(val, line_num);
    else if (key == "max_force")          sp.max_force = parse_float(val, line_num);
    else if (key == "min_speed")          sp.min_speed = parse_float(val, line_num);
    else if (key == "separation_weight")  sp.separation_weight = parse_float(val, line_num);
    else if (key == "alignment_weight")   sp.alignment_weight = parse_float(val, line_num);
    else if (key == "cohesion_weight")    sp.cohesion_weight = parse_float(val, line_num);
    else if (key == "separation_radius")  sp.separation_radius = parse_float(val, line_num);
    else if (key == "alignment_radius")   sp.alignment_radius = parse_float(val, line_num);
    else if (key == "cohesion_radius")    sp.cohesion_radius = parse_float(val, line_num);
    else if (key == "fov")                sp.fov = parse_float(val, line_num);
    else if (key == "noise_factor")       sp.noise_factor = parse_float(val, line_num);
    else return false;
    return true;
}

bool apply_global_field(SimConfig& config, const std::string& key,
                        const std::string& val, int line_num) {
    // Infection probabilities
    if (key == "p_initial_infect_normal")  { config.p_initial_infect_normal = parse_float(val, line_num); }
    else if (key == "p_initial_infect_doctor")  { config.p_initial_infect_doctor = parse_float(val, line_num); }
    else if (key == "p_infect_normal")          { config.p_infect_normal = parse_float(val, line_num); }
    else if (key == "p_infect_doctor")          { config.p_infect_doctor = parse_float(val, line_num); }
    else if (key == "p_spontaneous_infect")    { config.p_spontaneous_infect = parse_float(val, line_num); }
    // Cure
    else if (key == "p_cure")                   { config.p_cure = parse_float(val, line_num); }
    // Interaction radii
    else if (key == "r_interact_normal")        { config.r_interact_normal = parse_float(val, line_num); }
    else if (key == "r_interact_doctor")        { config.r_interact_doctor = parse_float(val, line_num); }
    // World bounds
    else if (key == "world_width")              { config.world_width = parse_float(val, line_num); }
    else if (key == "world_height")             { config.world_height = parse_float(val, line_num); }
    else if (key == "wall_bounce")              { config.wall_bounce = (val == "true" || val == "1"); }
    // Population (int)
    else if (key == "initial_normal_count")     { config.initial_normal_count = parse_int(val, line_num); }
    else if (key == "initial_doctor_count")     { config.initial_doctor_count = parse_int(val, line_num); }
    // Debuffs
    else if (key == "debuff_p_cure_infected")              { config.debuff_p_cure_infected = parse_float(val, line_num); }
    else if (key == "debuff_r_interact_doctor_infected")   { config.debuff_r_interact_doctor_infected = parse_float(val, line_num); }
    else if (key == "debuff_r_interact_normal_infected")   { config.debuff_r_interact_normal_infected = parse_float(val, line_num); }
    // Cure immunity
    else if (key == "cure_immunity_level")                 { config.cure_immunity_level = parse_float(val, line_num); }
    // Doctor seeking (global, not per-swarm)
    else if (key == "doctor_seek_radius")  { config.doctor_seek_radius = parse_float(val, line_num); }
    else if (key == "doctor_seek_weight")  { config.doctor_seek_weight = parse_float(val, line_num); }
    // Headless mode
    else if (key == "nogui")               { config.nogui = (val == "true" || val == "1"); }
    else if (key == "nogui_duration")      { config.nogui_duration = parse_float(val, line_num); }
    else if (key == "csv_sample_interval") { config.csv_sample_interval = parse_float(val, line_num); }
    else if (key == "headless_dt")         { config.headless_dt = parse_float(val, line_num); }
    // Output
    else if (key == "output_dir") {
        std::strncpy(config.output_dir, val.c_str(), sizeof(config.output_dir) - 1);
        config.output_dir[sizeof(config.output_dir) - 1] = '\0';
    }
    else {
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
    std::string current_section;

    while (std::getline(file, line)) {
        line_num++;
        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        // Track section headers
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
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
        std::string val = trimmed.substr(eq_pos + 1);

        // Strip inline comments (';' or '#' after value)
        auto comment_pos = val.find_first_of(";#");
        if (comment_pos != std::string::npos) {
            val = val.substr(0, comment_pos);
        }
        val = trim(val);

        if (key.empty() || val.empty()) {
            throw std::runtime_error(
                "config line " + std::to_string(line_num) +
                ": empty key or value");
        }

        // Route based on current section
        if (current_section == "normal_swarm") {
            if (apply_swarm_field(config.normal, key, val, line_num)) continue;
        } else if (current_section == "doctor_swarm") {
            if (apply_swarm_field(config.doctor, key, val, line_num)) continue;
            if (key == "behavior") {
                if (val == "normal") config.doctor_behavior = DoctorBehavior::Normal;
                else if (val == "nearest") config.doctor_behavior = DoctorBehavior::SeekNearest;
                else if (val == "centroid") config.doctor_behavior = DoctorBehavior::SeekCentroid;
                else std::cerr << "config warning: unknown doctor behavior '" << val << "'\n";
                continue;
            }
            if (key == "doctor_seek_radius") { config.doctor_seek_radius = parse_float(val, line_num); continue; }
            if (key == "doctor_seek_weight") { config.doctor_seek_weight = parse_float(val, line_num); continue; }
        }

        // Fall through to global field parsing
        if (!apply_global_field(config, key, val, line_num)) {
            std::cerr << "config warning: unknown key '" << key << "' on line "
                      << line_num << " (ignored)\n";
        }
    }

    return true;
}
