#include "renderer.h"
#include "render_config.h"
#include <raylib.h>
#include <cmath>
#include <flecs.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

// ============================================================
// Helper: Convert uint32_t RGBA to Raylib Color
// ============================================================

static Color uint32_to_color(uint32_t rgba) {
    return Color{
        static_cast<unsigned char>((rgba >> 0) & 0xFF),   // R
        static_cast<unsigned char>((rgba >> 8) & 0xFF),   // G
        static_cast<unsigned char>((rgba >> 16) & 0xFF),  // B
        static_cast<unsigned char>((rgba >> 24) & 0xFF)   // A
    };
}

// ============================================================
// Window lifecycle
// ============================================================

void init_renderer(int width, int height, const char* title) {
    InitWindow(width, height, title);
    SetTargetFPS(RenderConfig::FPS_TARGET);
}

void close_renderer() {
    CloseWindow();
}

// ============================================================
// Frame lifecycle
// ============================================================

void begin_frame() {
    BeginDrawing();
    ClearBackground(uint32_to_color(RenderConfig::COLOR_BACKGROUND));
}

void end_frame() {
    EndDrawing();
}

// ============================================================
// Draw primitives
// ============================================================

void draw_boid(float x, float y, float angle, uint32_t color, float radius) {
    const float length = RenderConfig::BOID_TRIANGLE_LENGTH;
    const float half_width = radius;

    // Calculate triangle points (pointing along the angle)
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);

    Vector2 tip = {
        x + cos_a * length,
        y + sin_a * length
    };

    Vector2 left = {
        x + std::cos(angle + RenderConfig::BOID_WING_ANGLE_OFFSET) * half_width,
        y + std::sin(angle + RenderConfig::BOID_WING_ANGLE_OFFSET) * half_width
    };

    Vector2 right = {
        x + std::cos(angle - RenderConfig::BOID_WING_ANGLE_OFFSET) * half_width,
        y + std::sin(angle - RenderConfig::BOID_WING_ANGLE_OFFSET) * half_width
    };

    DrawTriangle(tip, right, left, uint32_to_color(color));
}

void draw_interaction_radius(float x, float y, float radius, uint32_t color) {
    DrawCircleLines(static_cast<int>(x), static_cast<int>(y), radius, uint32_to_color(color));
}

// ============================================================
// Stats overlay with interactive controls
// ============================================================

void draw_stats_overlay(const SimStats& stats, void* world_ptr) {
    // Cast world pointer back to flecs::world (may be nullptr for demo)
    flecs::world* world = static_cast<flecs::world*>(world_ptr);
    SimConfig* config = nullptr;
    if (world) {
        config = &world->get_mut<SimConfig>();
    }

    // Draw stats panel
    GuiPanel(Rectangle{
        static_cast<float>(RenderConfig::STATS_PANEL_X),
        static_cast<float>(RenderConfig::STATS_PANEL_Y),
        static_cast<float>(RenderConfig::STATS_PANEL_WIDTH),
        450.0f  // Increased height for sliders
    }, "Simulation Stats & Controls");

    const int x = RenderConfig::STATS_PANEL_X + 10;
    int y = RenderConfig::STATS_PANEL_Y + 30;
    const int line_height = RenderConfig::STATS_LINE_HEIGHT;
    const int slider_width = 150;

    // --- Population stats ---
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("Normal Alive: %d", stats.normal_alive));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("Doctor Alive: %d", stats.doctor_alive));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("Dead Total: %d", stats.dead_total));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("  Normal: %d", stats.dead_normal));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("  Doctor: %d", stats.dead_doctor));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("Newborns Total: %d", stats.newborns_total));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("  Normal: %d", stats.newborns_normal));
    y += line_height;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20},
             TextFormat("  Doctor: %d", stats.newborns_doctor));
    y += line_height + 10;

    // --- Interactive sliders (only active if world is available) ---
    if (config) {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20}, "--- Live Controls ---");
        y += line_height;

        // p_infect_normal slider
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 100, 20}, "p_infect_normal");
        GuiSlider(
            Rectangle{static_cast<float>(x + 100), static_cast<float>(y), static_cast<float>(slider_width), 20},
            "0.0", "1.0", &config->p_infect_normal, 0.0f, 1.0f);
        y += line_height;

        // p_cure slider
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 100, 20}, "p_cure");
        GuiSlider(
            Rectangle{static_cast<float>(x + 100), static_cast<float>(y), static_cast<float>(slider_width), 20},
            "0.0", "1.0", &config->p_cure, 0.0f, 1.0f);
        y += line_height;

        // r_interact_normal slider
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 100, 20}, "r_interact_normal");
        GuiSlider(
            Rectangle{static_cast<float>(x + 100), static_cast<float>(y), static_cast<float>(slider_width), 20},
            "10", "100", &config->r_interact_normal, 10.0f, 100.0f);
        y += line_height;

        // r_interact_doctor slider
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 100, 20}, "r_interact_doctor");
        GuiSlider(
            Rectangle{static_cast<float>(x + 100), static_cast<float>(y), static_cast<float>(slider_width), 20},
            "10", "100", &config->r_interact_doctor, 10.0f, 100.0f);
        y += line_height;

        // Note: initial population counts are not editable during runtime
        // They only affect spawn_initial_population() which runs once at startup
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20},
                 TextFormat("Initial Normal: %d (startup)", config->initial_normal_count));
        y += line_height;

        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20},
                 TextFormat("Initial Doctor: %d (startup)", config->initial_doctor_count));
    } else {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20}, "--- Controls Disabled ---");
        y += line_height;
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20}, "(No world available)");
    }
}

// ============================================================
// Render complete frame
// ============================================================

void render_frame(const RenderState& state, void* world_ptr) {
    begin_frame();

    // Draw interaction radii first (background layer)
    for (const auto& boid : state.boids) {
        uint32_t radius_color = boid.is_doctor
            ? RenderConfig::COLOR_RADIUS_DOCTOR
            : RenderConfig::COLOR_RADIUS_NORMAL;
        draw_interaction_radius(boid.x, boid.y, boid.radius, radius_color);
    }

    // Draw boids on top
    for (const auto& boid : state.boids) {
        draw_boid(boid.x, boid.y, boid.angle, boid.color, RenderConfig::BOID_BASE_RADIUS);
    }

    // Draw stats overlay with interactive controls
    draw_stats_overlay(state.stats, world_ptr);

    end_frame();
}
