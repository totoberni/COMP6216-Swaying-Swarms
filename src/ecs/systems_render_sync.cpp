#include "systems.h"
#include "components.h"
#include "render_state.h"
#include "render/render_config.h"
#include <flecs.h>

// ============================================================
// OnStore Phase: Render Sync
// ============================================================

void register_render_sync_system(flecs::world& world) {
    world.system("RenderSyncSystem")
        .kind(flecs::OnStore)
        .run([](flecs::iter& it) {
            flecs::world w = it.world();
            const SimConfig& config = w.get<SimConfig>();
            RenderState& rs = w.get_mut<RenderState>();

            // Copy current stats
            rs.stats = w.get<SimStats>();

            // Clear previous frame data; use previous frame count as reserve hint
            size_t prev_count = rs.boids.size();
            rs.boids.clear();
            rs.boids.reserve(prev_count);

            // Populate config and sim_state pointers for renderer
            rs.config = &w.get_mut<SimConfig>();
            rs.sim_state = &w.get_mut<SimulationState>();

            // Build render data for all alive boids
            auto q = w.query<const Position, const Velocity, const Heading>();
            q.each([&](flecs::entity e, const Position& pos, const Velocity& vel,
                        const Heading& heading) {
                BoidRenderData brd;
                brd.x = pos.x;
                brd.y = pos.y;
                brd.angle = heading.angle;
                // Determine swarm type
                if (e.has<DoctorBoid>()) {
                    brd.swarm_type = 1;
                } else if (e.has<AntivaxBoid>()) {
                    brd.swarm_type = 2;
                } else {
                    brd.swarm_type = 0;
                }

                // Color: red if infected, else swarm-specific
                if (e.has<Infected>()) {
                    brd.color = RenderConfig::COLOR_INFECTED;
                } else if (brd.swarm_type == 1) {
                    brd.color = RenderConfig::COLOR_DOCTOR;
                } else if (brd.swarm_type == 2) {
                    brd.color = RenderConfig::COLOR_ANTIVAX;
                } else {
                    brd.color = RenderConfig::COLOR_NORMAL;
                }

                brd.radius = (brd.swarm_type == 1) ? config.r_interact_doctor : config.r_interact_normal;
                rs.boids.push_back(brd);
            });
        });
}
