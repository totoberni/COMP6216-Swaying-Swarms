#pragma once

#include <cstdint>
#include "components.h"
#include "render_state.h"

// ============================================================
// Renderer API — wraps Raylib rendering
// ============================================================

// Initialize the rendering window
void init_renderer(int width, int height, const char* title);

// Close the rendering window
void close_renderer();

// Begin a new frame (wraps BeginDrawing + ClearBackground)
void begin_frame();

// End the current frame (wraps EndDrawing)
void end_frame();

// Draw a single boid as a small triangle
void draw_boid(float x, float y, float angle, uint32_t color, float radius);

// Draw an interaction radius as a circle outline
void draw_interaction_radius(float x, float y, float radius, uint32_t color);

// Draw the stats overlay panel using raygui (includes interactive sliders)
// Requires world reference to update SimConfig in real-time
void draw_stats_overlay(const SimStats& stats, void* world_ptr);

// Render a complete frame from the provided render state
// Requires world pointer to support interactive UI elements
void render_frame(const RenderState& state, void* world_ptr);
