# CONTEXT:
Refer to the README.md and ToDo.md files in the codebase to gather context. 

## The Model
The repository aims to run simulation models in 2D of two boid swarms:

* Normal Boids Swarm
* Doctor Boids Swarm

### Behaviors and states
Both swarms should closely follow the Boid model for swarms, grouping with similar individuals, with slightly different rules. This is the model:

Normal Boids:
* At start of simulation have a small chance of becoming infected (p_initial_infect_normal)
* Infected boids die after some arbitrary parameter t_death 
* Infected boids have a 50% chance (p_infect_normal) of infecting other boids when a collision is registered (x,y coords of each boid are within a certain radius threshold r_interact_normal)
* Regardless of infection, boyds have a 40% chance of reproducing (p_offspring_normal) when a collision is registered (x,y coords of each boid are within a certain radius threshold r_interact), randomly generating a kid according to a normal distribution p_n_kids~N(2,1). Note this only applies to normal boid interactions, so no normal with doctor.
* IF two infected boids meet, and they infect each other and produce offspring, the child(ren) will be subject to the same contagion (p_infect_normal) from only ONE of the two parents.
* When a boid has lived beyond a certain lifespan (t_adult), they also have a 5% chance of becoming a doctor (p_become_doctor)

Doctor Boids: 
* Note this is a SEPARATE swarm from the normal boids, with its unique boid swarm parameters.
* They too start with a small chance of being infected (p_initial_infect_doctor)
* Infected doctors also die after arbitrary parameter t_death
* Infected doctor boids have a 50% chance (p_infect_doctor) of infecting other boids when a collision is registered (x,y coords of each boid are within a certain radius threshold r_interact)
* Regardless of infection, doctor boids always have an 80% chance (p_cure parameter) of curing sick boids when a collision is registered (x,y coords of each boid are within a certain radius threshold r_interact_doctor)
* Doctors can cure sick doctors. Doctors cannot cure healthy individuals of any swarm (nothing happens if no-one is infected during a collision)
* Regardless of infection, doctor boyds have a 5% chance of reproducing (p_offspring_doctor) when a collision is registered (x,y coords of each boid are within a certain radius threshold r_interact), randomly generating a doctor kid according to a normal distribution p_n_kids~N(1,1). Note this only applies only to doctor boid interactions, so no normal with doctor.
* IF two infected doctor boids meet, and they infect each other and produce offspring, the child(ren) will be subject to the same contagion (p_infect_doctor) from only ONE of the two parents.

### Simulation View
This is meant to be an interactive simulation, which is supported by a GUI that displays real life information regarding the simulation. This means that, when the simulation is started, some pane must be rendered in real-time with the simulation data. Things to track are: 

* \# normal boyds
* \# doctor boyds
* \# dead_total
* \# dead_normal
* \# dead_doctor
* \# newborns_total
* \# newborns_normal
* \# newborns_doctor
Styling these in a conveniently compact table is optimal for UI.

We also want to render all live boids (alive members of the two swarms), their movements, interaction radii as circles around them, and a background.

### Extensions to Behaviors
The model aims to study how changing these parameters affects the behaviors of the normal and doctor swarms. As such, we also want to leave some built-in space for later extensions:

* Infected Doctors have a debuffed behavior in params (less p_cure, less r_interact_doctor, less p_offspring_doctor)
* Infecter Normal have a debuffed behavior in params(less r_interact_normal, less p_offspring_normal)
* Sex has to be implemented for both swarms: providing a limitation to reproduction, which can only happen between male and female 
* A small percentage of the normal boids (p_antivax) are antivaxers: they actvely avoid doctors when they see them. This requires editing some of the boid rules for the normal swarm. 
* Later, obstacles will be built in the simulation, with a need to find the relevant libraries to render separate shapes concurrently

## Technologies

### Physics Engine
The physics engine should be as simple as possible, while allowing the 2D interaction mechanics to happen. We don't need a real engine per-se, just some logic that makes sense in 2D.

### Languages
This is where research must be done. So far, the team has agreed on: 
* C/C++/C# : for main language
* FLECS : A built in entity component system

### Missig
Missing techs most definetely are: 
* GUI interface libraries
* Colors and styled components like boxes and shapes to represent boids, background, obdtacles, colors and the such
* Dependency Management: This is critical since it's a shared GitHub project
* Sound libraries
* Miscellaneous components that I don't know yet
