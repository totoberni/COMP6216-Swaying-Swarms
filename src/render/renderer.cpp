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
    // Precompute wing offset trig once (constant across all boids)
    static const float cos_wing = std::cos(RenderConfig::BOID_WING_ANGLE_OFFSET);
    static const float sin_wing = std::sin(RenderConfig::BOID_WING_ANGLE_OFFSET);

    const float length = RenderConfig::BOID_TRIANGLE_LENGTH;
    const float half_width = radius;

    // Only 2 trig calls per boid instead of 6
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);

    Vector2 tip = {
        x + cos_a * length,
        y + sin_a * length
    };

    // Angle addition: cos(a+b) = cos_a*cos_b - sin_a*sin_b
    //                 sin(a+b) = sin_a*cos_b + cos_a*sin_b
    float cos_left = cos_a * cos_wing - sin_a * sin_wing;
    float sin_left = sin_a * cos_wing + cos_a * sin_wing;
    Vector2 left = {
        x + cos_left * half_width,
        y + sin_left * half_width
    };

    // cos(a-b) = cos_a*cos_b + sin_a*sin_b
    // sin(a-b) = sin_a*cos_b - cos_a*sin_b
    float cos_right = cos_a * cos_wing + sin_a * sin_wing;
    float sin_right = sin_a * cos_wing - cos_a * sin_wing;
    Vector2 right = {
        x + cos_right * half_width,
        y + sin_right * half_width
    };

    DrawTriangle(tip, right, left, uint32_to_color(color));
}

void draw_interaction_radius(float x, float y, float radius, uint32_t color) {
    DrawCircleLines(static_cast<int>(x), static_cast<int>(y), radius, uint32_to_color(color));
}

// Renders the average boid indicator: centroid circle + alignment arrow from same position
void draw_avg_boid_indicator(float x, float y, float radius, Vector2 vel_avg,
                             float arrow_length, uint32_t circle_color, uint32_t arrow_color) {
    DrawCircle(static_cast<int>(round(x)), static_cast<int>(round(y)), radius, uint32_to_color(circle_color));
    auto line_vec = Vector2Scale(Vector2Normalize(vel_avg), arrow_length);
    DrawLine(
        static_cast<int>(round(x)), static_cast<int>(round(y)),
        static_cast<int>(round(x + line_vec.x)), static_cast<int>(round(y + line_vec.y)),
        uint32_to_color(arrow_color)
    );
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

// Per-swarm line colors
static const Color SWARM_COLORS[3] = {
    {0, 230, 0, 230},    // Normal (green)
    {0, 120, 255, 230},  // Doctor (blue)
    {255, 165, 0, 230},  // Antivax (orange)
};
static const char* SWARM_NAMES[3] = {"Normal", "Doctor", "Antivax"};

static void draw_cohesion_graph(const SimStats& stats, int x, int y, int width, int height) {
    static float smoothed_max = 1.0f;

    DrawRectangle(x, y, width, height, Color{30, 30, 35, 255});
    DrawRectangleLines(x, y, width, height, Color{80, 80, 80, 255});

    // Check if any swarm has data
    int max_count = 0;
    for (int s = 0; s < 3; ++s)
        if (stats.swarm[s].coh_history_count > max_count) max_count = stats.swarm[s].coh_history_count;
    if (max_count < 2) {
        DrawText("Collecting data...", x + 5, y + height / 2 - 5, 10, LIGHTGRAY);
        return;
    }

    // Find current max across all swarms
    float current_max = 1.0f;
    for (int s = 0; s < 3; ++s)
        for (int i = 0; i < stats.swarm[s].coh_history_count; i++)
            if (stats.swarm[s].coh_history[i] > current_max) current_max = stats.swarm[s].coh_history[i];

    smoothed_max = std::fmax(smoothed_max * 0.99f, current_max);
    float max_coh = std::ceil(smoothed_max);

    float y_scale = static_cast<float>(height - 4) / max_coh;
    float x_scale = static_cast<float>(width - 4) / static_cast<float>(SwarmMetrics::HISTORY_SIZE - 1);

    // Grid lines
    Color grid_color = {60, 60, 65, 255};
    for (int pct = 25; pct <= 100; pct += 25) {
        float gy = y + height - 2 - (max_coh * pct / 100) * y_scale;
        for (int gx = x + 2; gx < x + width - 2; gx += 6)
            DrawLine(gx, static_cast<int>(gy), std::min(gx + 3, x + width - 2), static_cast<int>(gy), grid_color);
    }

    // Draw 3 swarm lines
    for (int s = 0; s < 3; ++s) {
        const auto& sm = stats.swarm[s];
        if (sm.coh_history_count < 2) continue;
        for (int i = 0; i < sm.coh_history_count - 1; i++) {
            int ri = (sm.coh_history_index - sm.coh_history_count + i + SwarmMetrics::HISTORY_SIZE) % SwarmMetrics::HISTORY_SIZE;
            int ni = (ri + 1) % SwarmMetrics::HISTORY_SIZE;
            float x1 = x + 2 + i * x_scale;
            float y1 = y + height - 2 - sm.coh_history[ri] * y_scale;
            float x2 = x + 2 + (i + 1) * x_scale;
            float y2 = y + height - 2 - sm.coh_history[ni] * y_scale;
            DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 1.5f, SWARM_COLORS[s]);
        }
    }

    // Legend (top-right, vertical)
    int lx = x + width - 60;
    int ly = y + 5;
    DrawRectangle(lx - 3, ly - 2, 62, 42, Color{20, 20, 25, 200});
    for (int s = 0; s < 3; ++s) {
        DrawRectangle(lx, ly, 8, 8, SWARM_COLORS[s]);
        DrawText(SWARM_NAMES[s], lx + 12, ly, 8, LIGHTGRAY);
        ly += 12;
    }

    DrawText(TextFormat("Max: %.2f", max_coh), x + width - 60, y - 12, 8, Color{130, 130, 130, 255});
}

static void draw_alignment_graph(const SimStats& stats, int x, int y, int width, int height) {
    DrawRectangle(x, y, width, height, Color{30, 30, 35, 255});
    DrawRectangleLines(x, y, width, height, Color{80, 80, 80, 255});

    int max_count = 0;
    for (int s = 0; s < 3; ++s)
        if (stats.swarm[s].ali_history_count > max_count) max_count = stats.swarm[s].ali_history_count;
    if (max_count < 2) {
        DrawText("Collecting data...", x + 5, y + height / 2 - 5, 10, LIGHTGRAY);
        return;
    }

    float half_range = 3.15f;
    float y_scale = static_cast<float>(height - 4) / (2.0f * half_range);
    float x_scale = static_cast<float>(width - 4) / static_cast<float>(SwarmMetrics::HISTORY_SIZE - 1);
    float center_y = y + height / 2.0f;

    // Center-line at zero
    Color zero_color = {100, 100, 105, 255};
    for (int gx = x + 2; gx < x + width - 2; gx += 6)
        DrawLine(gx, static_cast<int>(center_y), std::min(gx + 3, x + width - 2), static_cast<int>(center_y), zero_color);

    // Grid lines
    Color grid_color = {60, 60, 65, 255};
    for (int pct = 25; pct <= 100; pct += 25) {
        float offset = (half_range * pct / 100.0f) * y_scale;
        float gy_pos = center_y - offset;
        for (int gx = x + 2; gx < x + width - 2; gx += 6)
            DrawLine(gx, static_cast<int>(gy_pos), std::min(gx + 3, x + width - 2), static_cast<int>(gy_pos), grid_color);
        float gy_neg = center_y + offset;
        for (int gx = x + 2; gx < x + width - 2; gx += 6)
            DrawLine(gx, static_cast<int>(gy_neg), std::min(gx + 3, x + width - 2), static_cast<int>(gy_neg), grid_color);
    }

    // Draw 3 swarm lines
    for (int s = 0; s < 3; ++s) {
        const auto& sm = stats.swarm[s];
        if (sm.ali_history_count < 2) continue;
        for (int i = 0; i < sm.ali_history_count - 1; i++) {
            int ri = (sm.ali_history_index - sm.ali_history_count + i + SwarmMetrics::HISTORY_SIZE) % SwarmMetrics::HISTORY_SIZE;
            int ni = (ri + 1) % SwarmMetrics::HISTORY_SIZE;
            float x1 = x + 2 + i * x_scale;
            float y1 = center_y - sm.ali_history[ri] * y_scale;
            float x2 = x + 2 + (i + 1) * x_scale;
            float y2 = center_y - sm.ali_history[ni] * y_scale;
            DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 1.5f, SWARM_COLORS[s]);
        }
    }

    // Legend (top-right, vertical)
    int lx = x + width - 60;
    int ly = y + 5;
    DrawRectangle(lx - 3, ly - 2, 62, 42, Color{20, 20, 25, 200});
    for (int s = 0; s < 3; ++s) {
        DrawRectangle(lx, ly, 8, 8, SWARM_COLORS[s]);
        DrawText(SWARM_NAMES[s], lx + 12, ly, 8, LIGHTGRAY);
        ly += 12;
    }

    DrawText(TextFormat("Max: %.2f", half_range), x + width - 60, y - 12, 8, Color{130, 130, 130, 255});
}

static void draw_separation_graph(const SimStats& stats, int x, int y, int width, int height) {
    static float smoothed_max = 1.0f;

    DrawRectangle(x, y, width, height, Color{30, 30, 35, 255});
    DrawRectangleLines(x, y, width, height, Color{80, 80, 80, 255});

    int max_count = 0;
    for (int s = 0; s < 3; ++s)
        if (stats.swarm[s].sep_history_count > max_count) max_count = stats.swarm[s].sep_history_count;
    if (max_count < 2) {
        smoothed_max = 1.0f;
        DrawText("Collecting data...", x + 5, y + height / 2 - 5, 10, LIGHTGRAY);
        return;
    }

    float current_max = 1.0f;
    for (int s = 0; s < 3; ++s)
        for (int i = 0; i < stats.swarm[s].sep_history_count; i++)
            if (stats.swarm[s].sep_history[i] > current_max) current_max = stats.swarm[s].sep_history[i];

    smoothed_max = std::fmax(smoothed_max * 0.99f, current_max);
    float max_sep = std::ceil(smoothed_max);

    float y_scale = static_cast<float>(height - 4) / max_sep;
    float x_scale = static_cast<float>(width - 4) / static_cast<float>(SwarmMetrics::HISTORY_SIZE - 1);

    Color grid_color = {60, 60, 65, 255};
    for (int pct = 25; pct <= 100; pct += 25) {
        float gy = y + height - 2 - (max_sep * pct / 100) * y_scale;
        for (int gx = x + 2; gx < x + width - 2; gx += 6)
            DrawLine(gx, static_cast<int>(gy), std::min(gx + 3, x + width - 2), static_cast<int>(gy), grid_color);
    }

    // Draw 3 swarm lines
    for (int s = 0; s < 3; ++s) {
        const auto& sm = stats.swarm[s];
        if (sm.sep_history_count < 2) continue;
        for (int i = 0; i < sm.sep_history_count - 1; i++) {
            int ri = (sm.sep_history_index - sm.sep_history_count + i + SwarmMetrics::HISTORY_SIZE) % SwarmMetrics::HISTORY_SIZE;
            int ni = (ri + 1) % SwarmMetrics::HISTORY_SIZE;
            float x1 = x + 2 + i * x_scale;
            float y1 = y + height - 2 - sm.sep_history[ri] * y_scale;
            float x2 = x + 2 + (i + 1) * x_scale;
            float y2 = y + height - 2 - sm.sep_history[ni] * y_scale;
            DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 1.5f, SWARM_COLORS[s]);
        }
    }

    // Legend (top-right, vertical)
    int lx = x + width - 60;
    int ly = y + 5;
    DrawRectangle(lx - 3, ly - 2, 62, 42, Color{20, 20, 25, 200});
    for (int s = 0; s < 3; ++s) {
        DrawRectangle(lx, ly, 8, 8, SWARM_COLORS[s]);
        DrawText(SWARM_NAMES[s], lx + 12, ly, 8, LIGHTGRAY);
        ly += 12;
    }

    DrawText(TextFormat("Max: %.2f", max_sep), x + width - 60, y - 12, 8, Color{130, 130, 130, 255});
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
    s_slider_specs.push_back({"cure_immunity",   &config->cure_immunity_level,      0.0f,   1.0f, 1});

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
    const float panel_height = 1070.0f;
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
             TextFormat("N:%d  D:%d  A:%d",
                        stats.swarm[0].alive, stats.swarm[1].alive, stats.swarm[2].alive));
    y += line_height - 4;

    /*
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             TextFormat("Dead: %d (N:%d D:%d A:%d)",
                        stats.dead_total, stats.dead_normal, stats.dead_doctor, stats.dead_antivax));
    y += line_height - 4;

    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             TextFormat("Born: %d (N:%d D:%d A:%d)",
                        stats.newborns_total, stats.newborns_normal, stats.newborns_doctor, stats.newborns_antivax));
    y += line_height + 2;
    */

    // ========================================================
    // Average Metrics (bright values for readability)
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Average Metrics ---");
    y += line_height - 4;

    {
        Color label_col = {180, 180, 180, 255};
        Color value_col = {255, 255, 255, 255};
        int fs = 10;

        DrawText("Avg Cohesion:", x, y + 2, fs, label_col);
        DrawText(TextFormat("N:%.1f  D:%.1f  A:%.1f",
                 stats.swarm[0].average_cohesion,
                 stats.swarm[1].average_cohesion,
                 stats.swarm[2].average_cohesion),
                 x + 90, y + 2, fs, value_col);
        y += line_height - 4;

        DrawText("Avg Alignment:", x, y + 2, fs, label_col);
        DrawText(TextFormat("N:%.2f  D:%.2f  A:%.2f",
                 stats.swarm[0].average_alignment_angle,
                 stats.swarm[1].average_alignment_angle,
                 stats.swarm[2].average_alignment_angle),
                 x + 95, y + 2, fs, value_col);
        y += line_height - 4;

        DrawText("Avg Sep (RMS):", x, y + 2, fs, label_col);
        DrawText(TextFormat("N:%.1f  D:%.1f  A:%.1f",
                 stats.swarm[0].average_separation,
                 stats.swarm[1].average_separation,
                 stats.swarm[2].average_separation),
                 x + 95, y + 2, fs, value_col);
        y += line_height - 4;
    }

    // ========================================================
    // Controls section: dropdown + sliders
    // ========================================================
    // Dropdown is drawn DEFERRED (after graphs) so expanded items render on top.
    int dropdown_y = 0;  // saved for deferred drawing
    if (config) {
        GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
                 "--- Controls ---");
        y += line_height - 2;

        // Save position for deferred dropdown drawing
        dropdown_y = y;
        y += 28;  // always advance past dropdown rect

        // Draw sliders (only when dropdown is closed)
        if (!s_dropdown_edit_mode) {
            for (std::size_t i = 0; i < s_slider_specs.size(); ++i) {
                const SliderSpec& spec = s_slider_specs[i];
                if (spec.category != s_active_category) continue;

                GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y),
                                    static_cast<float>(label_width - 40), 16},
                         spec.label);
                // Draw value text right-aligned within the label area
                DrawText(TextFormat("%.2f", *spec.value),
                         x + label_width - 35, y + 2, 10, Color{180, 180, 180, 255});
                GuiSliderBar(
                    Rectangle{static_cast<float>(x + label_width + 5), static_cast<float>(y),
                              static_cast<float>(slider_width), 16},
                    "", "",
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
    const int graph_width = RenderConfig::STATS_PANEL_WIDTH - 20;
    const int graph_height = 150;
    /*GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Population History ---");
    y += line_height + 14;
    draw_population_graph(stats, x, y, graph_width, graph_height);
    y += graph_height + 8;*/

    // ========================================================
    // Cohesion Graph
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Cohesion History ---");
    y += line_height + 14;
    draw_cohesion_graph(stats, x, y, graph_width, graph_height);
    y += graph_height + 8;

    // ========================================================
    // Alignment Graph
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Alignment Angle History ---");
    y += line_height + 14;
    draw_alignment_graph(stats, x, y, graph_width, graph_height);
    y += graph_height + 8;

    // ========================================================
    // Separation Graph
    // ========================================================
    GuiLabel(Rectangle{static_cast<float>(x), static_cast<float>(y), 280, 20},
             "--- Separation History ---");
    y += line_height + 14;
    draw_separation_graph(stats, x, y, graph_width, graph_height);
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

    // ========================================================
    // Deferred dropdown rendering (drawn LAST so items appear on top)
    // ========================================================
    if (config && dropdown_y > 0) {
        const char* categories = "Infection;Cure;Reproduction;Transition;Interaction;Movement;Debuffs;Antivax;Time";
        Rectangle dropdown_rect = {static_cast<float>(x), static_cast<float>(dropdown_y),
                                    static_cast<float>(RenderConfig::STATS_PANEL_WIDTH - 20), 24};
        if (s_dropdown_edit_mode) {
            if (GuiDropdownBox(dropdown_rect, categories, &s_active_category, true)) {
                s_dropdown_edit_mode = false;
            }
        } else {
            if (GuiDropdownBox(dropdown_rect, categories, &s_active_category, false)) {
                s_dropdown_edit_mode = true;
            }
        }
    }
}

// ============================================================
// Render complete frame
// ============================================================

void render_frame(const RenderState& state) {
    begin_frame();

    // Draw interaction radii (background layer) — toggled with V key, off by default
    if (state.sim_state && state.sim_state->show_radii) {
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
    }

    // Draw per-swarm centroid indicators (circle + alignment arrow)
    {
        static const uint32_t centroid_colors[3] = {
            0x5500FF00,  // Normal: semi-transparent green
            0x5578B4FF,  // Doctor: semi-transparent blue
            0x5500A5FF,  // Antivax: semi-transparent orange
        };
        static const uint32_t arrow_colors[3] = {
            0xFF00FF00,  // Normal: green
            0xFFFF7800,  // Doctor: blue
            0xFF00A5FF,  // Antivax: orange
        };
        for (int s = 0; s < 3; ++s) {
            const auto& sm = state.stats.swarm[s];
            if (sm.alive > 0) {
                draw_avg_boid_indicator(sm.pos_avg.x, sm.pos_avg.y, 8.f,
                                        sm.vel_avg, 20.f,
                                        centroid_colors[s], arrow_colors[s]);
            }
        }
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
