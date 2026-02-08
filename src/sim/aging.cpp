#include "aging.h"

void age_entity(float& age, float dt) {
    age += dt;
}

void tick_infection(float& time_infected, float dt) {
    time_infected += dt;
}
