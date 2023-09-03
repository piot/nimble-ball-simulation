/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_BALL_SIMULATION_H
#define NIMBLE_BALL_SIMULATION_H

#define NL_MAX_PLAYERS (16)
#define NL_MAX_PARTICIPANTS (16)

#include <basal/circle.h>
#include <basal/line_segment.h>
#include <basal/rect.h>
#include <basal/rect2.h>
#include <basal/vector2.h>
#include <clog/clog.h>
#include <stdbool.h>
#include <stddef.h>

static const uint8_t NL_TEAM_UNDEFINED = 0xff;
static const uint8_t NL_AVATAR_INDEX_UNDEFINED = 0xff;

typedef struct NlGoal {
    int ownedByTeam;
    BlRect rect;
    bool facingLeft;
} NlGoal;

typedef struct NlConstants {
    NlGoal goals[2];
    BlLineSegment borderSegments[6];
    uint16_t matchDurationInTicks;
} NlConstants;

extern const NlConstants g_nlConstants;

typedef struct NlPlayerInGameInput {
    int8_t verticalAxis;
    int8_t horizontalAxis;
    uint8_t buttons;
} NlPlayerInGameInput;

typedef struct NlPlayerSelectTeam {
    uint8_t preferredTeamToJoin;
} NlPlayerSelectTeam;

typedef enum NlPlayerInputType {
    NlPlayerInputTypeForced,
    NlPlayerInputTypeWaitingForReconnect,
    NlPlayerInputTypeNone,
    NlPlayerInputTypeInGame,
    NlPlayerInputTypeSelectTeam,
} NlPlayerInputType;

typedef struct NlPlayerInput {
    uint8_t inputType;
    union {
        NlPlayerInGameInput inGameInput;
        NlPlayerSelectTeam selectTeam;
    } input;
} NlPlayerInput;

typedef struct NlPlayerInputWithParticipantInfo {
    uint8_t participantId;
    NlPlayerInput playerInput;
} NlPlayerInputWithParticipantInfo;

typedef struct NlParticipant {
    uint8_t participantId;
    uint8_t playerIndex;
    bool isUsed;
    bool internalMarked;
} NlParticipant;

typedef enum NlPlayerPhase {
    NlPlayerPhaseSelectTeam,
    NlPlayerPhaseCommittedToTeam,
    NlPlayerPhasePlaying
} NlPlayerPhase;

typedef struct NlPlayer {
    uint8_t playerIndex;
    uint8_t preferredTeamId;
    uint8_t controllingAvatarIndex;
    uint8_t assignedToParticipantIndex;
    NlPlayerInput playerInput;
    NlPlayerPhase phase;
    bool isWaitingForReconnect;
} NlPlayer;

#define NL_MAX_TEAMS (2)

typedef struct NlTeam {
    uint8_t score;
} NlTeam;

typedef struct NlTeams {
    NlTeam teams[NL_MAX_TEAMS];
    uint8_t teamCount;
} NlTeams;

typedef struct NlPlayers {
    NlPlayer players[NL_MAX_PLAYERS];
    uint8_t playerCount;
} NlPlayers;

typedef struct NlAvatar {
    uint8_t avatarIndex;
    BlCircle circle;
    BlVector2 requestedVelocity;
    BlVector2 velocity;
    float visualRotation;
    uint8_t controlledByPlayerIndex;
    uint8_t dribbleCooldown;
    uint8_t kickCooldown;
    uint8_t kickedCounter;
    uint8_t slideTackleCooldown;
    uint8_t slideTackleRemainingTicks;

    float slideTackleRotation;
    bool requestBuildKickPower;
    bool requestSlideTackle;
    uint8_t kickPower;
    uint8_t teamIndex;
} NlAvatar;

typedef struct NlAvatars {
    NlAvatar avatars[NL_MAX_PLAYERS];
    uint8_t avatarCount;
} NlAvatars;

typedef enum NlGamePhase {
    NlGamePhaseWaitingForPlayers,
    NlGamePhaseCountDown,
    NlGamePhasePlaying,
    NlGamePhaseAfterAGoal,
    NlGamePhasePostGame,
} NlGamePhase;

typedef struct NlBall {
    BlCircle circle;
    BlVector2 velocity;
    uint8_t collideCounter;
} NlBall;

typedef struct NlGame {
    NlParticipant participantLookup[NL_MAX_PARTICIPANTS];
    uint8_t lastParticipantLookupCount;
    NlPlayers players;
    NlAvatars avatars;
    NlTeams teams;
    NlBall ball;
    uint8_t phase;
    uint16_t phaseCountDown;
    uint16_t tickCount;
    uint16_t matchClockLeftInTicks;
    uint8_t latestScoredTeamIndex;
} NlGame;

void nlGameInit(NlGame* self);
void nlGameTick(NlGame* self, const NlPlayerInputWithParticipantInfo* inputs, size_t inputCount, Clog* log);
const NlPlayer* nlGameFindSimulationPlayerFromParticipantId(const NlGame* self, uint8_t participantId);

#endif
