/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_BALL_SIMULATION_H
#define NIMBLE_BALL_SIMULATION_H

#define NL_MAX_PLAYERS (16)
#define NL_MAX_PARTICIPANTS (32)

#include <clog/clog.h>
#include <stdbool.h>
#include <stddef.h>
#include <basal/vector2.h>
#include <basal/basal_rect2.h>
#include <basal/circle.h>
#include <basal/rect.h>
#include <basal/basal_line_segment.h>

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
    int verticalAxis;
    int horizontalAxis;
    bool passButton;
    bool exertButton;
} NlPlayerInGameInput;

typedef struct NlPlayerSelectTeam {
    int preferredTeamToJoin;
} NlPlayerSelectTeam;

typedef enum NlPlayerInputType {
    NlPlayerInputTypeNone,
    NlPlayerInputTypeInGame,
    NlPlayerInputTypeSelectTeam,
} NlPlayerInputType;

typedef struct NlPlayerInput {
    uint8_t participantId;
    NlPlayerInputType inputType;
    union {
        NlPlayerInGameInput inGameInput;
        NlPlayerSelectTeam selectTeam;
    } input;
} NlPlayerInput;

typedef struct NlParticipant {
    int participantId;
    size_t playerIndex;
    bool isUsed;
    bool internalMarked;
} NlParticipant;


typedef struct NlPlayer {
    int preferredTeamId;
    int controllingAvatarIndex;
    int assignedToParticipantIndex;
    NlPlayerInput playerInput;
} NlPlayer;

#define NL_MAX_TEAMS (2)

typedef struct NlTeam {
    uint8_t score;
} NlTeam;

typedef struct NlTeams {
    NlTeam teams[NL_MAX_TEAMS];
    size_t teamCount;
} NlTeams;

typedef struct NlPlayers {
    NlPlayer players[NL_MAX_PLAYERS];
    size_t playerCount;
} NlPlayers;

typedef struct NlAvatar {
    BlCircle circle;
    BlVector2 requestedVelocity;
    BlVector2 velocity;
    float visualRotation;
    size_t controlledByPlayerIndex;
    uint8_t dribbleCooldown;
    uint8_t kickCooldown;
    bool requestBuildKickPower;
    uint8_t kickPower;
    uint8_t teamIndex;
} NlAvatar;

typedef struct NlAvatars {
    NlAvatar avatars[NL_MAX_PLAYERS];
    size_t avatarCount;
} NlAvatars;

typedef enum NlGamePhase {
    NlGamePhaseWaitingForPlayers,
    NlGamePhaseCountDown,
    NlGamePhasePlaying,
    NlGamePhaseAfterAGoal,
    NlGamePhasePostGame,
} NlGamePhase;

typedef struct NlArena {
    int halfLineX;
} NlArena;

typedef struct NlBall {
    BlCircle circle;
    BlVector2 velocity;
} NlBall;


typedef struct NlGame {
    NlParticipant participantLookup[NL_MAX_PARTICIPANTS];
    size_t lastParticipantLookupCount;
    NlPlayers players;
    NlTeams teams;
    NlAvatars avatars;
    NlGamePhase phase;
    uint16_t phaseCountDown;
    NlArena arena;
    NlBall ball;
    size_t tickCount;
    uint16_t matchClockLeftInTicks;
    uint8_t latestScoredTeamIndex;
} NlGame;

void nlGameInit(NlGame* self);
void nlGameTick(NlGame* self, const NlPlayerInput* inputs, size_t inputCount, Clog* log);

#endif
