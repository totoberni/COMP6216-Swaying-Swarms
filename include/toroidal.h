#pragma once
#include <cmath>

// Shortest signed displacement from `from` to `to` on periodic axis [0, period).
// Returns value in (-period/2, +period/2].
inline float torus_diff(float from, float to, float period) {
    float d = to - from;
    float half = period * 0.5f;
    if (d > half) d -= period;
    if (d < -half) d += period;
    return d;
}

// Toroidal squared distance between two 2D points.
inline float torus_dist_sq(float ax, float ay, float bx, float by,
                           float world_w, float world_h) {
    float dx = torus_diff(ax, bx, world_w);
    float dy = torus_diff(ay, by, world_h);
    return dx * dx + dy * dy;
}

// Displacement vector from (ax,ay) to (bx,by).
// Toroidal when toroidal=true, Euclidean otherwise.
// This is the ONE function all systems use — abstracts the choice.
inline void displacement(float ax, float ay, float bx, float by,
                         bool toroidal, float world_w, float world_h,
                         float& dx, float& dy) {
    if (toroidal) {
        dx = torus_diff(ax, bx, world_w);
        dy = torus_diff(ay, by, world_h);
    } else {
        dx = bx - ax;
        dy = by - ay;
    }
}
