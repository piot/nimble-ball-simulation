/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_BALL_SIMULATION_VM_H
#define NIMBLE_BALL_SIMULATION_VM_H

#include <nimble-ball-simulation/nimble_ball_simulation.h>
#include <transmute/transmute.h>

typedef struct NlSimulationVm {
    TransmuteVm transmuteVm;
    NlGame game;
    Clog log;
} NlSimulationVm;

void nlSimulationVmInit(NlSimulationVm* self, Clog log);

#endif
