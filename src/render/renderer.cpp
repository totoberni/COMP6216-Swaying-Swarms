#include "renderer.h"
#include "render_config.h"
#include <raylib.h>
#include <cmath>

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
// Population graph helper
// ============================================================

static void draw_population_graph(const SimStats& stats, int x, int y, int width, int height) {
    // Smoothed max for stable Y-axis scaling
    static float smoothed_max = 1.0f;

    // Draw graph background
    DrawRectangle(x, y, width, height, Color{40, 40, 40, 255});
    DrawRectangleLines(x, y, width, height, Color{100, 100, 100, 255});

    if (stats.history_count < 2) {
        // Not enough data to draw - reset smoothed max
        smoothed_max = 1.0f;
        DrawText("Collecting data...", x + 5, y + height / 2 - 5, 10, LIGHTGRAY);
        return;
    }

    // Find current max population
    int current_max = 1;  // Avoid division by zero
    for (int i = 0; i < stats.history_count; i++) {
        int total = stats.history[i].normal_alive + stats.history[i].doctor_alive;
        if (total > current_max) {
            current_max = total;
        }
    }

    // Apply smoothing: decay existing max, but snap up immediately to new peaks
    smoothed_max = std::max(smoothed_max * 0.99f, static_cast<float>(current_max));

    int max_pop = static_cast<int>(std::ceil(smoothed_max));

    // Scale factor for Y axis
    float y_scale = static_cast<float>(height - 4) / static_cast<float>(max_pop);
    float x_scale = static_cast<float>(width - 4) / static_cast<float>(SimStats::HISTORY_SIZE - 1);

    // Draw lines for Normal population (green)
    for (int i = 0; i < stats.history_count - 1; i++) {
        int read_index = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int next_read_index = (read_index + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[read_index].normal_alive * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[next_read_index].normal_alive * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 2.0f, Color{0, 255, 0, 255});  // Green for normal
    }

    // Draw lines for Doctor population (blue)
    for (int i = 0; i < stats.history_count - 1; i++) {
        int read_index = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int next_read_index = (read_index + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[read_index].doctor_alive * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[next_read_index].doctor_alive * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 2.0f, Color{0, 120, 255, 255});  // Blue for doctor
    }

    // Draw legend
    DrawRectangle(x + 5, y + 5, 10, 10, Color{0, 255, 0, 255});
    DrawText("Normal", x + 20, y + 5, 10, LIGHTGRAY);
    DrawRectangle(x + 75, y + 5, 10, 10, Color{0, 120, 255, 255});
    DrawText("Doctor", x + 90, y + 5, 10, LIGHTGRAY);

    // Draw max value label
    DrawText(TextFormat("Max: %d", max_pop), x + width - 50, y + 5, 10, LIGHTGRAY);
}

// ============================================================
// Stats overlay with interactive controls
// ============================================================

void draw_stats_overlay(const RenderState& state) {
    const SimStats& stats = state.stats;
    SimConfig* config = state.config;
    SimulationState* sim_state = state.sim_state;

    // Draw stats panel
    const float panel_height = 680.0f;  // Increased height for buttons, sliders, and graph
    GuiPanel(Rectangle{
        static_cast<float>(RenderConfig::STATS_PANEL_X),
        static_cast<float>(RenderConfig::STATS_PANEL_Y),
        static_cast<float>(RenderConfig::STATS_PANEL_WIDTH),
        panel_height
    }, "Simulation Stats & Controls");

    const int x = RenderConfig::STATS_PANEL_X + 10;
    int y = RenderConfig::STATS_PANEL_Y + 30;
    const int line_height = RenderConfig::STATS_LINE_HEIGHT;
    const int slider_width = 150;
    const int button_width = 100;
    const int button_height = 30;

    // --- Pause/Reset Controls ---
    if (sim_state) {
        // Pause button
        const char* pause_text = sim_state->is_paused ? "Resume (SPACE)" : "Pause (SPACE)";
        if (GuiButton(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      pause_text)) {
            sim_state->is_paused = !sim_state->is_paused;
        }

        // Reset button (next to Pause button)
        if (GuiButton(Rectangle{static_cast<float>(x + button_width + 10), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      "Reset (R)")) {
            sim_state->reset_requested = true;
        }
        y += button_height + 10;
    }

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
            "5", "200", &config->r_interact_normal, 5.0f, 200.0f);
        y += line_height;

        // r_interact_doctor slider
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 100, 20}, "r_interact_doctor");
        GuiSlider(
            Rectangle{static_cast<float>(x + 100), static_cast<float>(y), static_cast<float>(slider_width), 20},
            "5", "200", &config->r_interact_doctor, 5.0f, 200.0f);
        y += line_height;

        // Note: initial population counts are not editable during runtime
        // They only affect spawn_initial_population() which runs once at startup
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20},
                 TextFormat("Initial Normal: %d (startup)", config->initial_normal_count));
        y += line_height;

        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20},
                 TextFormat("Initial Doctor: %d (startup)", config->initial_doctor_count));
        y += line_height + 10;
    } else {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20}, "--- Controls Disabled ---");
        y += line_height;
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 220, 20}, "(No world available)");
        y += line_height + 10;
    }

    // --- Population Graph ---
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 200, 20}, "--- Population History ---");
    y += line_height;
    const int graph_width = RenderConfig::STATS_PANEL_WIDTH - 20;
    const int graph_height = 120;
    draw_population_graph(stats, x, y, graph_width, graph_height);
}

// ============================================================
// Render complete frame
// ============================================================

void render_frame(const RenderState& state) {
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
    draw_stats_overlay(state);

    end_frame();
}
