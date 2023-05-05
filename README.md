# Nimble Ball Simulation

Example soccer-like game with a [Transmute](https://github.com/piot/transmute-c)-wrapper so it can be easily used in a deterministic game simulation.

It does not contain any rendering code, only simulation.

All simulation code is in [nimble_ball_simulation.c](src/lib/nimble_ball_simulation.c) and the wrapper is in [nimble_ball_simulation_vm.c](src/lib/nimble_ball_simulation_vm.c)

Depends on [Basal](https://github.com/piot/basal-c) for math functions and types.
