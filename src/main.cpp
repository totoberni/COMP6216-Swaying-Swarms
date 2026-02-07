#include <iostream>
#include <flecs.h>

int main() {
    std::cout << "[DEBUG] Starting...\n" << std::flush;

    std::cout << "[DEBUG] Creating world...\n" << std::flush;
    flecs::world world;

    std::cout << "[DEBUG] World created successfully!\n" << std::flush;

    return 0;
}
