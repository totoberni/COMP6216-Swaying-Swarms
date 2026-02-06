# Master ToDo File

## TODOs:

[] : Build Dependency System for C and FLEX\
[] : Implement Normal Boid swarm + rules\
[] : Implement Doctor Boid swarm + rules\
[] : Simulate and fuck around\

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