#include "cure.h"

bool try_cure(float p_cure, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < p_cure;
}
