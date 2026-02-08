#include "reproduction.h"
#include <algorithm>
#include <cmath>

int offspring_count(float mean, float stddev, std::mt19937& rng) {
    std::normal_distribution<float> dist(mean, stddev);
    float count = dist(rng);
    return std::max(0, static_cast<int>(std::round(count)));
}

bool try_reproduce(float p_offspring, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < p_offspring;
}
