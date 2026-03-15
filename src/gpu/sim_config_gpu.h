#pragma once

#include <cstdint>

struct SwarmParamsGpu {
    float cohesion_weight, alignment_weight, separation_weight;
    float cohesion_radius, alignment_radius, separation_radius;
    float fov, noise_factor, max_speed, max_force, min_speed;
};

struct SimConfigGpu {
    SwarmParamsGpu normal, doctor;
    float world_w, world_h;
    bool wall_bounce;
    // SIR
    float p_infect_normal, p_infect_doctor;
    float p_spontaneous_infect, p_cure;
    float r_interact_normal, r_interact_doctor;
    float debuff_p_cure_infected;
    float debuff_r_interact_doctor_infected;
    float debuff_r_interact_normal_infected;
    float cure_immunity_level;
    // Doctor seeking
    int doctor_behavior;  // 0=normal, 1=seek_nearest, 2=seek_centroid
    float doctor_seek_radius, doctor_seek_weight;
};
