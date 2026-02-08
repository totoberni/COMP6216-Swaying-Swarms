# Changelog — render
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
- **Pause/Reset controls**: Added Pause/Resume and Reset buttons to stats overlay in renderer.cpp. Pause button toggles SimulationState.is_paused. Reset button sets SimulationState.reset_requested (handled by main loop). Stats panel height increased to 530px to accommodate new controls. Buttons are 100px wide, placed at top of panel.
- **Parameter sliders**: Added interactive raygui sliders for p_infect_normal, p_cure, r_interact_normal, r_interact_doctor in stats overlay. Sliders update SimConfig singleton in real-time. Added world pointer parameter to render_frame() and draw_stats_overlay(). Stats panel height increased to 450px to accommodate controls.

## 2026-02-07

- **21:03Z** | `src/render/render_config.h` | File written/created | session:`c24a9544`
- **21:03Z** | `src/render/renderer.h` | File written/created | session:`c24a9544`
- **21:04Z** | `src/render/renderer.cpp` | File written/created | session:`c24a9544`
- **21:04Z** | `src/render/render_demo.cpp` | File written/created | session:`c24a9544`
- **21:06Z** | `src/render/render_demo.cpp` | Edited: '#include "renderer.h" #include "render_config.h" #include <r' -> '#include "renderer.h" #include "render_config.h" #include <r' | session:`c24a9544`
- **21:08Z** | `src/render/render_config.h` | Edited: '// =========================================================' -> '// =========================================================' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.h` | Edited: '#pragma once  #include <cstdint> #include "../../include/com' -> '#pragma once  #include <cstdint> #include "components.h" #in' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: 'void init_renderer(int width, int height, const char* title)' -> 'void init_renderer(int width, int height, const char* title)' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: '    Vector2 left = {         x + std::cos(angle + 2.5f) * ha' -> '    Vector2 left = {         x + std::cos(angle + RenderConf' | session:`c24a9544`
- **21:08Z** | `src/render/renderer.cpp` | Edited: '    const int x = RenderConfig::STATS_PANEL_X + 10;     int ' -> '    const int x = RenderConfig::STATS_PANEL_X + 10;     int ' | session:`c24a9544`
