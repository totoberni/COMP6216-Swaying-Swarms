#include <flecs.h>
#include <raylib.h>
#include "components.h"

int main() {
    flecs::world world;
    world.set<SimConfig>({});
    world.set<SimStats>({});

    InitWindow(800, 600, "Boid Swarm - COMP6216");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawText("Boid Swarm Simulation", 240, 280, 30, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
