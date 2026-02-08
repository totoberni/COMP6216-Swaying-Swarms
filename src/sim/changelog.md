# Changelog — sim
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->

## 2026-02-08
- **Infected debuffs implemented**: Added debuff multipliers to SimConfig for infected boids
  - Normal boids: r_interact_normal ×0.8, p_offspring_normal ×0.5
  - Doctor boids: r_interact_doctor ×0.7, p_cure ×0.5, p_offspring_doctor ×0.5
  - Applied in infection, cure, and reproduction systems
