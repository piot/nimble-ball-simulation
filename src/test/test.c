/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "utest.h"
#include <clog/clog.h>
#include <nimble-ball-simulation/nimble_ball_simulation.h>

UTEST(NimbleBall, verify)
{
    NlGame game;

    nlGameInit(&game);
    ASSERT_EQ(0, game.avatars.avatarCount);
    ASSERT_EQ(NlGamePhaseWaitingForPlayers, game.phase);

    NlPlayerInputWithParticipantInfo singleInput;
    singleInput.participantId = 3;
    singleInput.playerInput.input.inGameInput.horizontalAxis = -99;

    Clog subLog;

    subLog.config = &g_clog;
    subLog.constantPrefix = "NimbleBall";

    nlGameTick(&game, &singleInput, 1, &subLog);
    ASSERT_EQ(NlGamePhaseCountDown, game.phase);
    ASSERT_EQ(1, game.players.playerCount);
    ASSERT_EQ(1, game.avatars.avatarCount);

    nlGameTick(&game, &singleInput, 1, &subLog);
    ASSERT_EQ(NlGamePhaseCountDown, game.phase);
    ASSERT_EQ(62 * 3 - 1, game.phaseCountDown);
}
