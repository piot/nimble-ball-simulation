/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "utest.h"
#include <clog/clog.h>
#include <nimble-ball-simulation/nimble_ball_simulation_vm.h>

UTEST(NimbleBall, testvm)
{
    NlSimulationVm simulationVm;

    Clog subLog;

    subLog.config = &g_clog;
    subLog.constantPrefix = "NimbleBallVm";

    nlSimulationVmInit(&simulationVm, subLog);
    TransmuteVm* vm = &simulationVm.transmuteVm;

    NlGame initialGameState;
    nlGameInit(&initialGameState);

    TransmuteState initState;
    initState.octetSize = sizeof(NlGame);
    initState.state = &initialGameState;

    transmuteVmSetState(vm, &initState);

    TransmuteState firstState = transmuteVmGetState(vm);

    const int tempBufSize = 64;
    char tempBuf[tempBufSize];
    transmuteVmStateToString(vm, &firstState, tempBuf, tempBufSize);
    CLOG_VERBOSE("state: %s", tempBuf)

    TransmuteInput input;

    NlPlayerInput playerInputs[2];
    playerInputs[0].inputType = NlPlayerInputTypeInGame;
    playerInputs[0].input.inGameInput.horizontalAxis = 33;

    playerInputs[1].inputType = NlPlayerInputTypeSelectTeam;
    playerInputs[1].input.selectTeam.preferredTeamToJoin = 1;

    TransmuteParticipantInput participantInputs[2];
    participantInputs[0].octetSize = sizeof(NlPlayerInput);
    participantInputs[0].input = &playerInputs[0];
    participantInputs[0].participantId = 13;

    participantInputs[1].octetSize = sizeof(NlPlayerInput);
    participantInputs[1].input = &playerInputs[1];
    participantInputs[1].participantId = 2;

    input.participantCount = 2;
    input.participantInputs = participantInputs;

    ASSERT_EQ(0, simulationVm.game.tickCount);

    transmuteVmTick(vm, &input);

    ASSERT_EQ(1, simulationVm.game.tickCount);
}
