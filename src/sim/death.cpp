#include "death.h"

bool should_die(float time_infected, float t_death) {
    return time_infected >= t_death;
}
