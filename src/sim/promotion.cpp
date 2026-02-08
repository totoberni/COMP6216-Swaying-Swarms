#include "promotion.h"

bool try_promote(float age, float t_adult, float p_become_doctor, std::mt19937& rng) {
    if (age < t_adult) {
        return false;
    }

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < p_become_doctor;
}
