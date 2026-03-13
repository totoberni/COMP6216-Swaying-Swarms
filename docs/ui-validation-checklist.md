# UI Validation Checklist

Run: `./build/boid_swarm` (default config)

## Buttons
- [ ] Pause/Resume toggles correctly (also SPACE key)
- [ ] Reset (R) respawns boids, clears graphs
- [ ] Hide (H) hides the stats overlay (second row, no overlap with Reset)
- [ ] H keyboard shortcut toggles overlay
- [ ] No button overlaps or unclickable buttons

## Graph Selector
- [ ] Dropdown opens without visual glitches
- [ ] "None" selected: no graph area shown, panel is shorter
- [ ] Each of the 9 graph options renders a graph
- [ ] Population graph shows 3 colored lines (green/blue/red)
- [ ] Infected graph shows rising red line (infection spreads)
- [ ] % Infected shows 0-100% scale
- [ ] Growth Rate shows centered Y-axis (positive and negative)
- [ ] Recovered shows green line
- [ ] Graph dropdown items render ON TOP of graph content
- [ ] Switching graphs doesn't leave visual artifacts

## Controls
- [ ] Controls dropdown shows 5 categories (Infection, Cure, Interaction, Movement, Debuffs)
- [ ] Sliders respond to drag
- [ ] Slider values update in real-time
- [ ] Controls dropdown renders on top of graph area when expanded

## CSV Export
- [ ] Pause simulation, "Export CSV" button appears
- [ ] Click Export: feedback message "Exported to sim-out/outN/" appears clearly (not garbled)
- [ ] Feedback fades out after ~2 seconds
- [ ] Exported CSV in sim-out/outN/ has data rows (not just header)
- [ ] Resume simulation, Export button disappears

## Panel Fit
- [ ] Panel fits within 1080px window height (no clipping at bottom)
- [ ] With graph selected + Movement sliders (worst case): panel content doesn't overflow
- [ ] Without graph: no large empty space at bottom

## Per-Swarm Behavior (run with `configs/B1_D1.ini`)
- [ ] Normal boids form a line (narrow FOV)
- [ ] Doctor boids flock independently
- [ ] Boid speeds match config values (not degraded to 180)

## Known Acceptable Behaviors
- After Reset, graph Y-axis scale decays slowly (~7s) from previous peak — static `smoothed_max` persists across resets
- B1 and B2 configs both use `behavior = normal`; the analysis script maps both to B1
