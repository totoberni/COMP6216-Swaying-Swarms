#include "renderer.h"
#include "render_config.h"
#include <raylib.h>
#include <cmath>
#include <fstream>
#include <vector>
#include <cstddef>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

// CSV export state
static float export_feedback_timer = 0.0f;
static const char* export_feedback_text = "";

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

static bool export_population_csv(const SimStats& stats) {
    std::ofstream file("population_data.csv");
    if (!file.is_open()) return false;

    file << "frame,normal,doctor,antivax,infected\n";

    for (int i = 0; i < stats.history_count; i++) {
        int read_index = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        const auto& pt = stats.history[read_index];
        file << (i + 1) << "," << pt.normal_alive << "," << pt.doctor_alive
             << "," << pt.antivax_alive << "," << pt.infected_count << "\n";
    }

    return file.good();
}

static void draw_population_graph(const SimStats& stats, int x, int y, int width, int height) {
    // Smoothed max for stable Y-axis scaling
    static float smoothed_max = 1.0f;

    // Graph title
    const char* title = "Population Over Time";
    int title_width = MeasureText(title, 10);
    DrawText(title, x + (width - title_width) / 2, y - 14, 10, LIGHTGRAY);

    // Draw graph background
    DrawRectangle(x, y, width, height, Color{30, 30, 35, 255});
    DrawRectangleLines(x, y, width, height, Color{80, 80, 80, 255});

    if (stats.history_count < 2) {
        smoothed_max = 1.0f;
        DrawText("Collecting data...", x + 5, y + height / 2 - 5, 10, LIGHTGRAY);
        return;
    }

    // Find current max population
    int current_max = 1;
    for (int i = 0; i < stats.history_count; i++) {
        int total = stats.history[i].normal_alive + stats.history[i].doctor_alive + stats.history[i].antivax_alive;
        if (total > current_max) current_max = total;
        if (stats.history[i].infected_count > current_max) current_max = stats.history[i].infected_count;
    }

    smoothed_max = std::max(smoothed_max * 0.99f, static_cast<float>(current_max));
    int max_pop = static_cast<int>(std::ceil(smoothed_max));

    float y_scale = static_cast<float>(height - 4) / static_cast<float>(max_pop);
    float x_scale = static_cast<float>(width - 4) / static_cast<float>(SimStats::HISTORY_SIZE - 1);

    // Horizontal grid lines at 25%, 50%, 75%, 100%
    Color grid_color = {60, 60, 65, 255};
    for (int pct = 25; pct <= 100; pct += 25) {
        float gy = y + height - 2 - (max_pop * pct / 100) * y_scale;
        // Dotted line: draw short segments with gaps
        for (int gx = x + 2; gx < x + width - 2; gx += 6) {
            DrawLine(gx, static_cast<int>(gy), std::min(gx + 3, x + width - 2), static_cast<int>(gy), grid_color);
        }
        DrawText(TextFormat("%d%%", pct), x + 3, static_cast<int>(gy) - 9, 8, Color{90, 90, 90, 255});
    }

    // Draw infected line first (behind population lines) with transparency
    Color infected_color = {255, 60, 60, 180};
    for (int i = 0; i < stats.history_count - 1; i++) {
        int ri = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int ni = (ri + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[ri].infected_count * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[ni].infected_count * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 1.5f, infected_color);
    }

    // Normal population (green)
    Color normal_color = {0, 230, 0, 230};
    for (int i = 0; i < stats.history_count - 1; i++) {
        int ri = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int ni = (ri + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[ri].normal_alive * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[ni].normal_alive * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 2.0f, normal_color);
    }

    // Doctor population (blue)
    Color doctor_color = {0, 120, 255, 230};
    for (int i = 0; i < stats.history_count - 1; i++) {
        int ri = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int ni = (ri + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[ri].doctor_alive * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[ni].doctor_alive * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 2.0f, doctor_color);
    }

    // Antivax population (orange)
    Color antivax_color = {255, 165, 0, 230};
    for (int i = 0; i < stats.history_count - 1; i++) {
        int ri = (stats.history_index - stats.history_count + i + SimStats::HISTORY_SIZE) % SimStats::HISTORY_SIZE;
        int ni = (ri + 1) % SimStats::HISTORY_SIZE;

        float x1 = x + 2 + i * x_scale;
        float y1 = y + height - 2 - stats.history[ri].antivax_alive * y_scale;
        float x2 = x + 2 + (i + 1) * x_scale;
        float y2 = y + height - 2 - stats.history[ni].antivax_alive * y_scale;

        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 2.0f, antivax_color);
    }

    // Legend (top-right, vertical)
    int lx = x + width - 68;
    int ly = y + 5;
    DrawRectangle(lx - 3, ly - 2, 70, 56, Color{20, 20, 25, 200});
    DrawRectangle(lx, ly, 8, 8, normal_color);
    DrawText("Normal", lx + 12, ly, 8, LIGHTGRAY);
    ly += 12;
    DrawRectangle(lx, ly, 8, 8, doctor_color);
    DrawText("Doctor", lx + 12, ly, 8, LIGHTGRAY);
    ly += 12;
    DrawRectangle(lx, ly, 8, 8, antivax_color);
    DrawText("Antivax", lx + 12, ly, 8, LIGHTGRAY);
    ly += 12;
    DrawRectangle(lx, ly, 8, 8, infected_color);
    DrawText("Infected", lx + 12, ly, 8, LIGHTGRAY);

    // Max value label (top-left)
    DrawText(TextFormat("Max: %d", max_pop), x + 3, y + 3, 8, Color{130, 130, 130, 255});
}

// ============================================================
// Stats overlay with interactive controls (dropdown categories)
// ============================================================

struct SliderSpec {
    const char* label;
    float* value;
    float min_val;
    float max_val;
    int category;
};

static std::vector<SliderSpec> s_slider_specs;
static SimConfig* s_last_config = nullptr;
static int s_active_category = 0;
static bool s_dropdown_edit_mode = false;

static void build_slider_specs(SimConfig* config) {
    s_slider_specs.clear();
    s_last_config = config;

    // Category 0: Infection
    s_slider_specs.push_back({"p_init_inf_nrm", &config->p_initial_infect_normal,  0.0f,   1.0f, 0});
    s_slider_specs.push_back({"p_init_inf_doc", &config->p_initial_infect_doctor,  0.0f,   1.0f, 0});
    s_slider_specs.push_back({"p_infect_nrm",   &config->p_infect_normal,          0.0f,   1.0f, 0});
    s_slider_specs.push_back({"p_infect_doc",   &config->p_infect_doctor,          0.0f,   1.0f, 0});

    // Category 1: Cure
    s_slider_specs.push_back({"p_cure",          &config->p_cure,                   0.0f,   1.0f, 1});

    // Category 2: Reproduction
    s_slider_specs.push_back({"p_offspr_nrm",   &config->p_offspring_normal,       0.0f,   1.0f, 2});
    s_slider_specs.push_back({"p_offspr_doc",   &config->p_offspring_doctor,       0.0f,   1.0f, 2});
    s_slider_specs.push_back({"mean_offspr_nrm", &config->offspring_mean_normal,   0.0f,  10.0f, 2});
    s_slider_specs.push_back({"std_offspr_nrm", &config->offspring_stddev_normal,  0.0f,   5.0f, 2});
    s_slider_specs.push_back({"mean_offspr_doc", &config->offspring_mean_doctor,   0.0f,  10.0f, 2});
    s_slider_specs.push_back({"std_offspr_doc", &config->offspring_stddev_doctor,  0.0f,   5.0f, 2});
    s_slider_specs.push_back({"repro_cooldown", &config->reproduction_cooldown,    0.0f,  30.0f, 2});

    // Category 3: Transition
    s_slider_specs.push_back({"p_become_doc",   &config->p_become_doctor,          0.0f,   1.0f, 3});
    s_slider_specs.push_back({"p_antivax",      &config->p_antivax,               0.0f,   1.0f, 3});

    // Category 4: Interaction
    s_slider_specs.push_back({"r_interact_nrm", &config->r_interact_normal,        1.0f, 200.0f, 4});
    s_slider_specs.push_back({"r_interact_doc", &config->r_interact_doctor,        1.0f, 200.0f, 4});

    // Category 5: Movement
    s_slider_specs.push_back({"max_speed",      &config->max_speed,               10.0f, 500.0f, 5});
    s_slider_specs.push_back({"max_force",      &config->max_force,               10.0f, 500.0f, 5});
    s_slider_specs.push_back({"min_speed",      &config->min_speed,                0.0f, 500.0f, 5});
    s_slider_specs.push_back({"sep_weight",     &config->separation_weight,        0.0f,   5.0f, 5});
    s_slider_specs.push_back({"align_weight",   &config->alignment_weight,         0.0f,   5.0f, 5});
    s_slider_specs.push_back({"cohes_weight",   &config->cohesion_weight,          0.0f,   5.0f, 5});
    s_slider_specs.push_back({"sep_radius",     &config->separation_radius,        1.0f, 200.0f, 5});
    s_slider_specs.push_back({"align_radius",   &config->alignment_radius,         1.0f, 200.0f, 5});
    s_slider_specs.push_back({"cohes_radius",   &config->cohesion_radius,          1.0f, 200.0f, 5});

    // Category 6: Debuffs
    s_slider_specs.push_back({"db_p_cure",      &config->debuff_p_cure_infected,            0.0f, 2.0f, 6});
    s_slider_specs.push_back({"db_r_int_doc",   &config->debuff_r_interact_doctor_infected, 0.0f, 2.0f, 6});
    s_slider_specs.push_back({"db_p_off_doc",   &config->debuff_p_offspring_doctor_infected, 0.0f, 2.0f, 6});
    s_slider_specs.push_back({"db_r_int_nrm",   &config->debuff_r_interact_normal_infected, 0.0f, 2.0f, 6});
    s_slider_specs.push_back({"db_p_off_nrm",   &config->debuff_p_offspring_normal_infected, 0.0f, 2.0f, 6});

    // Category 7: Antivax
    s_slider_specs.push_back({"av_repul_radius", &config->antivax_repulsion_radius, 1.0f, 300.0f, 7});
    s_slider_specs.push_back({"av_repul_weight", &config->antivax_repulsion_weight, 0.0f,  10.0f, 7});

    // Category 8: Time
    s_slider_specs.push_back({"t_death",        &config->t_death,                  0.5f,  30.0f, 8});
    s_slider_specs.push_back({"t_adult",        &config->t_adult,                  0.5f,  60.0f, 8});
}

void draw_stats_overlay(const RenderState& state) {
    const SimStats& stats = state.stats;
    SimConfig* config = state.config;
    SimulationState* sim_state = state.sim_state;

    // Rebuild slider specs if config pointer changed (e.g. after reset)
    if (config && config != s_last_config) {
        build_slider_specs(config);
    }

    // Draw panel background
    const float panel_height = 850.0f;
    GuiPanel(Rectangle{
        static_cast<float>(RenderConfig::STATS_PANEL_X),
        static_cast<float>(RenderConfig::STATS_PANEL_Y),
        static_cast<float>(RenderConfig::STATS_PANEL_WIDTH),
        panel_height
    }, "Simulation Stats & Controls");

    const int x = RenderConfig::STATS_PANEL_X + 10;
    int y = RenderConfig::STATS_PANEL_Y + 30;
    const int line_height = RenderConfig::STATS_LINE_HEIGHT;
    const int button_width = 130;
    const int button_height = 28;
    const int slider_width = 180;
    const int label_width = 95;

    // ========================================================
    // Pause / Reset buttons
    // ========================================================
    if (sim_state) {
        const char* pause_text = sim_state->is_paused ? "Resume (SPACE)" : "Pause (SPACE)";
        if (GuiButton(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      pause_text)) {
            sim_state->is_paused = !sim_state->is_paused;
        }

        if (GuiButton(Rectangle{static_cast<float>(x + button_width + 10), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      "Reset (R)")) {
            sim_state->reset_requested = true;
        }

        if (GuiButton(Rectangle{static_cast<float>(x + button_width + 10), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      "Hide (H)")) {
            sim_state->show_stats_overlay = false;
        }
        y += button_height + 8;
    }

    // ========================================================
    // Compressed population stats (2 dense lines)
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Population ---");
    y += line_height - 4;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             TextFormat("Normal: %d  Doctor: %d  Antivax: %d",
                        stats.normal_alive, stats.doctor_alive, stats.antivax_alive));
    y += line_height - 4;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             TextFormat("Dead: %d (N:%d D:%d A:%d)",
                        stats.dead_total, stats.dead_normal, stats.dead_doctor, stats.dead_antivax));
    y += line_height - 4;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             TextFormat("Born: %d (N:%d D:%d A:%d)",
                        stats.newborns_total, stats.newborns_normal, stats.newborns_doctor, stats.newborns_antivax));
    y += line_height + 2;

    // ========================================================
    // Controls section: dropdown + sliders
    // ========================================================
    if (config) {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
                 "--- Controls ---");
        y += line_height - 2;

        // Category dropdown
        const char* categories = "Infection;Cure;Reproduction;Transition;Interaction;Movement;Debuffs;Antivax;Time";
        Rectangle dropdown_rect = {static_cast<float>(x), static_cast<float>(y),
                                    static_cast<float>(RenderConfig::STATS_PANEL_WIDTH - 20), 24};

        // When dropdown is open, skip sliders to avoid z-overlap
        if (s_dropdown_edit_mode) {
            // Draw dropdown on top (edit mode = open)
            if (GuiDropdownBox(dropdown_rect, categories, &s_active_category, true)) {
                s_dropdown_edit_mode = false;
            }
        } else {
            // Draw dropdown (closed)
            if (GuiDropdownBox(dropdown_rect, categories, &s_active_category, false)) {
                s_dropdown_edit_mode = true;
            }
            y += 28;

            // Draw sliders for the active category
            for (std::size_t i = 0; i < s_slider_specs.size(); ++i) {
                const SliderSpec& spec = s_slider_specs[i];
                if (spec.category != s_active_category) continue;

                GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                    static_cast<float>(label_width), 20},
                         spec.label);
                GuiSlider(
                    Rectangle{static_cast<float>(x + label_width + 5), static_cast<float>(y),
                              static_cast<float>(slider_width), 20},
                    "", TextFormat("%.2f", *spec.value),
                    spec.value, spec.min_val, spec.max_val);
                y += line_height;
            }

            // Cross-parameter guard: min_speed <= max_speed
            if (s_active_category == 5) {
                if (config->min_speed > config->max_speed) {
                    config->min_speed = config->max_speed;
                }
            }
        }

        y += 6;
    } else {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
                 "--- Controls Disabled ---");
        y += line_height;
    }

    // ========================================================
    // Population graph
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Population History ---");
    y += line_height + 14;
    const int graph_width = RenderConfig::STATS_PANEL_WIDTH - 20;
    const int graph_height = 150;
    draw_population_graph(stats, x, y, graph_width, graph_height);
    y += graph_height + 8;

    // ========================================================
    // CSV Export (only when paused)
    // ========================================================
    if (sim_state && sim_state->is_paused) {
        if (GuiButton(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(button_width), static_cast<float>(button_height)},
                      "Export CSV")) {
            if (export_population_csv(stats)) {
                export_feedback_text = "Exported to population_data.csv!";
            } else {
                export_feedback_text = "Export failed!";
            }
            export_feedback_timer = 2.0f;
        }
    }

    // Show export feedback
    if (export_feedback_timer > 0.0f) {
        export_feedback_timer -= GetFrameTime();
        unsigned char alpha = static_cast<unsigned char>(255 * std::min(1.0f, export_feedback_timer));
        DrawText(export_feedback_text, x, y + button_height + 4, 10, Color{200, 255, 200, alpha});
    }
}

// ============================================================
// Render complete frame
// ============================================================

void render_frame(const RenderState& state) {
    begin_frame();

    // Draw interaction radii first (background layer)
    for (const auto& boid : state.boids) {
        uint32_t radius_color;
        if (boid.swarm_type == 1) {
            radius_color = RenderConfig::COLOR_RADIUS_DOCTOR;
        } else if (boid.swarm_type == 2) {
            radius_color = RenderConfig::COLOR_RADIUS_ANTIVAX;
        } else {
            radius_color = RenderConfig::COLOR_RADIUS_NORMAL;
        }
        draw_interaction_radius(boid.x, boid.y, boid.radius, radius_color);
    }

    // Draw boids on top
    for (const auto& boid : state.boids) {
        draw_boid(boid.x, boid.y, boid.angle, boid.color, RenderConfig::BOID_BASE_RADIUS);
    }

    // Draw stats overlay with interactive controls
    if (state.sim_state && state.sim_state->show_stats_overlay) {
        draw_stats_overlay(state);
    }

    end_frame();
}
