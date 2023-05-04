/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-ball-simulation/nimble_ball_simulation_vm.h>

static TransmuteState getState(const void* _self)
{
    TransmuteState state;

    NlSimulationVm* self = (NlSimulationVm*) _self;

    state.octetSize = sizeof(NlGame);
    state.state = (const void*) &self->game;

    return state;
}

static void setState(void* _self, const TransmuteState* state)
{
    NlSimulationVm* self = (NlSimulationVm*) _self;

    CLOG_ASSERT(sizeof(NlGame) == state->octetSize, "transmute state size is wrong %zu", state->octetSize)

    self->game = *((const NlGame*) state->state);
}

static int stateToString(void* _self, const TransmuteState* state, char* target, size_t maxTargetOctetSize)
{
    (void) _self;

    const NlGame* appState = (NlGame*) state->state;
    return tc_snprintf(target, maxTargetOctetSize, "state: tick: %zu ball-pos: %.1f, %.1f", appState->tickCount,
                       appState->ball.circle.center.x, appState->ball.circle.center.y);
}

static int inputToString(void* _self, const TransmuteParticipantInput* input, char* target, size_t maxTargetOctetSize)
{
    (void) _self;
    const NlPlayerInput* participantInput = (NlPlayerInput*) input->input;
    switch (participantInput->inputType) {
        case NlPlayerInputTypeSelectTeam:
            return tc_snprintf(target, maxTargetOctetSize, "input: select team: %d",
                               participantInput->input.selectTeam.preferredTeamToJoin);
        case NlPlayerInputTypeNone:
            return tc_snprintf(target, maxTargetOctetSize, "input: none");
        case NlPlayerInputTypeInGame:
            return tc_snprintf(target, maxTargetOctetSize, "input: inGame: horizontalAxis: %d",
                               participantInput->input.inGameInput.horizontalAxis);
        default:
            CLOG_ERROR("unknown input type %d", participantInput->inputType)
    }
}

static void tick(void* _self, const TransmuteInput* input)
{
    NlSimulationVm* self = (NlSimulationVm*) _self;

    NlPlayerInputWithParticipantInfo playerInputs[64];

    for (size_t i = 0; i < input->participantCount; ++i) {
        if (input->participantInputs[i].octetSize == 0) {
            // This is a forced step. Represent that in a game specific way
            tc_mem_clear_type(&playerInputs[i].playerInput);
            playerInputs[i].playerInput.inputType = NlPlayerInputTypeForced;
        } else {
            CLOG_ASSERT(sizeof(NlPlayerInput) == input->participantInputs[i].octetSize, "wrong NlPlayerInput struct");
            playerInputs[i].playerInput = *(const NlPlayerInput*) input->participantInputs[i].input;
        }
        playerInputs[i].participantId = input->participantInputs[i].participantId;
    }

    nlGameTick(&self->game, playerInputs, input->participantCount, &self->log);
}

void nlSimulationVmInit(NlSimulationVm* self, Clog log)
{
    TransmuteVmSetup transmuteVmSetup;

    transmuteVmSetup.version.major = 0;
    transmuteVmSetup.version.minor = 0;
    transmuteVmSetup.version.patch = 43;
    transmuteVmSetup.inputToString = inputToString;
    transmuteVmSetup.stateToString = stateToString;
    transmuteVmSetup.getStateFn = getState;
    transmuteVmSetup.setStateFn = setState;
    transmuteVmSetup.tickDurationMs = 16;
    transmuteVmSetup.tickFn = tick;
    self->log = log;

    transmuteVmInit(&self->transmuteVm, self, transmuteVmSetup, log);
}
