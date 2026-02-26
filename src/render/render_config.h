#pragma once

#include <cstdint>

// ============================================================
// Visual constants for the renderer
// ============================================================

namespace RenderConfig {
    // --- Colors (ABGR format: 0xAABBGGRR) ---
    constexpr uint32_t COLOR_NORMAL      = 0xFF00FF00;  // Green
    constexpr uint32_t COLOR_DOCTOR      = 0xFFFF8800;  // Orange/amber
    constexpr uint32_t COLOR_INFECTED    = 0xFF0000FF;  // Red
    constexpr uint32_t COLOR_ANTIVAX     = 0xFF00A5FF;  // Orange (ABGR)
    constexpr uint32_t COLOR_BACKGROUND  = 0xFF1E1E1E;  // Dark gray

    constexpr uint32_t COLOR_MASS_CENTER = 0xFFBF40BF; // Purple

    // --- Interaction radius colors (semi-transparent) ---
    constexpr uint32_t COLOR_RADIUS_NORMAL = 0x4000FF00;  // Semi-transparent green
    constexpr uint32_t COLOR_RADIUS_DOCTOR = 0x40FF8800;  // Semi-transparent orange
    constexpr uint32_t COLOR_RADIUS_ANTIVAX = 0x4000A5FF;  // Semi-transparent orange

    // --- Boid visual properties ---
    constexpr float BOID_BASE_RADIUS = 5.0f;
    constexpr float BOID_TRIANGLE_LENGTH = 10.0f;
    constexpr float BOID_WING_ANGLE_OFFSET = 2.5f;

    // --- Rendering parameters ---
    constexpr int FPS_TARGET = 60;

    // --- Stats overlay ---
    constexpr int STATS_PANEL_X = 10;
    constexpr int STATS_PANEL_Y = 10;
    constexpr int STATS_PANEL_WIDTH = 300;
    constexpr int STATS_PANEL_HEIGHT = 200;
    constexpr int STATS_FONT_SIZE = 20;
    constexpr int STATS_LINE_HEIGHT = 25;
}
