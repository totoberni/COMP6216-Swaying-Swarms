# Changelog — render
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
- **21:08Z** | `src/render/renderer.h` | Edited: '#pragma once  #include <cstdint> #include "../../include/com' -> '#pragma once  #include <cstdint> #include "components.h" #in' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: 'void init_renderer(int width, int height, const char* title)' -> 'void init_renderer(int width, int height, const char* title)' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: '    Vector2 left = {         x + std::cos(angle + 2.5f) * ha' -> '    Vector2 left = {         x + std::cos(angle + RenderConf' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: '    const int x = RenderConfig::STATS_PANEL_X + 10;     int ' -> '    const int x = RenderConfig::STATS_PANEL_X + 10;     int ' | session:`c24a9544`

## 2026-02-18

- **00:04Z** | `src/render/render_config.h` | Edited: '    constexpr uint32_t COLOR_INFECTED    = 0xFF0000FF;  // R' -> '    constexpr uint32_t COLOR_INFECTED    = 0xFF0000FF;  // R' | session:`f655d95a`
- **00:05Z** | `src/render/render_config.h` | Edited: '    constexpr uint32_t COLOR_RADIUS_DOCTOR = 0x40FF8800;  //' -> '    constexpr uint32_t COLOR_RADIUS_DOCTOR = 0x40FF8800;  //' | session:`f655d95a`
- **00:05Z** | `src/render/renderer.cpp` | Edited: '        uint32_t radius_color = boid.is_doctor             ?' -> '        uint32_t radius_color;         if (boid.swarm_type =' | session:`f655d95a`
- **00:06Z** | `src/render/render_demo.cpp` | Edited: 'struct DemoBoid {     float x, y;     float vx, vy;     floa' -> 'struct DemoBoid {     float x, y;     float vx, vy;     floa' | session:`f655d95a`
- **00:07Z** | `src/render/render_demo.cpp` | Edited: '        // 10% doctors, 90% normal         boid.is_doctor = ' -> '        // 10% doctors, 10% antivax, 80% normal         if (' | session:`f655d95a`
- **00:07Z** | `src/render/render_demo.cpp` | Edited: '            render_boid.is_doctor = boid.is_doctor; ' -> '            render_boid.swarm_type = boid.swarm_type; ' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' -> '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' -> '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' -> '    GuiLabel(Rectangle{static_cast<float>(x), static_cast<fl' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    const float panel_height = 680.0f;  // Increased height ' -> '    const float panel_height = 755.0f;  // Increased height ' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    // Find current max population     int current_max = 1; ' -> '    // Find current max population     int current_max = 1; ' | session:`f655d95a`
- **00:15Z** | `src/render/renderer.cpp` | Edited: '    // Draw lines for Doctor population (blue)     for (int ' -> '    // Draw lines for Doctor population (blue)     for (int ' | session:`f655d95a`

## 2026-02-19

- **00:33Z** | `src/render/renderer.cpp` | Edited: '#include "renderer.h" #include "render_config.h" #include <r' -> '#include "renderer.h" #include "render_config.h" #include <r' | session:`4dfbd6b1`
- **00:34Z** | `src/render/renderer.cpp` | Edited: 'static void draw_population_graph(const SimStats& stats, int' -> 'static bool export_population_csv(const SimStats& stats) {  ' | session:`4dfbd6b1`
- **00:34Z** | `src/render/renderer.cpp` | Edited: '    // --- Population Graph ---     GuiLabel(Rectangle{stati' -> '    // --- Population Graph ---     GuiLabel(Rectangle{stati' | session:`4dfbd6b1`
- **00:34Z** | `src/render/renderer.cpp` | Edited: '    const float panel_height = 755.0f;  // Increased height ' -> '    const float panel_height = 830.0f;  // Height for button' | session:`4dfbd6b1`
- **01:32Z** | `src/render/render_config.h` | Edited: '    constexpr int STATS_PANEL_WIDTH = 250; ' -> '    constexpr int STATS_PANEL_WIDTH = 300; ' | session:`414c9a66`
- **01:32Z** | `src/render/renderer.cpp` | Edited: '#include "renderer.h" #include "render_config.h" #include <r' -> '#include "renderer.h" #include "render_config.h" #include <r' | session:`414c9a66`
- **01:34Z** | `src/render/renderer.cpp` | Edited: 'void draw_stats_overlay(const RenderState& state) {     cons' -> '// =========================================================' | session:`414c9a66`
- **01:34Z** | `src/render/renderer.cpp` | Edited: '// =========================================================' -> '// =========================================================' | session:`414c9a66`
