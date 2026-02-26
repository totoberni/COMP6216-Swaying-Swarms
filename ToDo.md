# Master ToDo File
<!-- For HUMAN developers, IGNORE if you're an agent!-->
## TODOs:

[ ] : Build Dependency System for C and FLEX\
[ ] : Implement Normal Boid swarm + rules\
[ ] : Implement Doctor Boid swarm + rules\
[ ] : Simulate and fuck around\

## Languages
* C/C# 

## Technologies and libraries
* Fast Lightweight Enity Component System (FLECS)

## System States
* Two swarms: Doctors and Normal Boids

### Boid States
We store states in **matrices**, handled as **tables** by FLECS:
- **Normal Boids**: Matrix with rows = properties, columns = Normal_Boid_ID
- **Doctor Boids**: Matrix with rows = properties, columns = Doctor_Boid_ID
- **Interactions**: Matrix with rows = Doctor_ID, Columns = Doctor_Boid_ID. Cells represent interactions.


## Iteration #2

### Extensions
- Modify simulation to follow specific country / world map + emulate covid / pandemic evolution with statistical data against pop graphs
- [Kathir]: Implement a FOV for boids (120 deg.) instead of checking alignment cohesion and separation in circle radius
- Model boid behaviors, updated as a function of population (boids should become more "shy" when more of them are sick)
   Behavior ideas:
   * Sick boids seek out doctors (exclude antivax, they should always escape doctors)
   * Antivax actively seek out infected boids to grow natural immunity
   * Boids become more "shy" (higher sep weight, lower cohesion) the more boids are sick
   * Normal boids exclude/avoid sick boids
- [Alberto]: Use raylib for vector management! Currently handled by codebase alone with no libs. Also update cone radius tests after merge.
- [David]: Find some linear decay of the force and velocity updates (helps with smooth update rules)
- Implement hospitals areas of some kind.

### Fixes
- Cooldown for sickness after cure. This is natural immunity. Requires: sickness applies a chance of death and chance of natural heal every second. Regardless of when boids heal, they have a time-dependent decaying immunity (from 100 immunity to 0) lasting 10s.Prevents swarms from being constantly sick. All of these interactions MUST have their configurable parameters in config.ini.
- Infection is currently being checked every frame (60/s), two fixes: check infection every second (edit the check to respect time int) or edit the infection rate
- Cooldown for sickness after cure. This is natural immunity. Requires: sickness applies a chance of death and chance of natural heal every second. Regardless of when boids heal, they have a time-dependent decaying immunity (from 100 immunity to 0) lasting 10s.Prevents swarms from being constantly sick.
- [Bryan] Make the population graph toggleable
- The control pane should be collapsable to provide full view of simulation
- The control pane has overflowing sliders to be fixed (overflow into simulation box)
- Control pane sliders should be circle+line not square+rectangle 

## Iteration #3 

- [Kathir]: Implement avg cohesion , avg alignment
- [Albe + Bry]: Implement avg separation with the centroid calculation
- [Albe]: Vectorize all with raylib & handle vectors
- [Albe]: Optimize runtime, clean all claude MDs and redocument