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
#include <basal/basal_vector2.h>
#include <basal/basal_rect2.h>
#include <basal/basal_circle.h>

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

typedef struct NlPlayers {
    NlPlayer players[NL_MAX_PLAYERS];
    size_t playerCount;
} NlPlayers;

typedef struct NlAvatar {
    bl_circle circle;
    bl_vector2 requestedVelocity;
    bl_vector2 velocity;
    float visualRotation;
    size_t controlledByPlayerIndex;
    uint8_t kickCooldown;
    bool kickWhenAble;
} NlAvatar;

typedef struct NlAvatars {
    NlAvatar avatars[NL_MAX_PLAYERS];
    size_t avatarCount;
} NlAvatars;

typedef enum NlGamePhase {
    NlGamePhaseWaitingForPlayers,
    NlGamePhaseCountDown,
    NlGamePhasePlaying,
    NlGamePhasePostGame,
} NlGamePhase;

typedef struct NlArena {
    bl_recti rect;
    int halfLineX;
} NlArena;

typedef struct NlBall {
    bl_circle circle;
    bl_vector2 velocity;
} NlBall;

typedef struct NlGame {
    NlParticipant participantLookup[NL_MAX_PARTICIPANTS];
    size_t lastParticipantLookupCount;
    NlPlayers players;
    NlAvatars avatars;
    NlGamePhase phase;
    NlArena arena;
    int countDown;
    NlBall ball;
    size_t tickCount;
} NlGame;

void nlGameInit(NlGame* self);
void nlGameTick(NlGame* self, const NlPlayerInput* inputs, size_t inputCount, Clog* log);

#endif
