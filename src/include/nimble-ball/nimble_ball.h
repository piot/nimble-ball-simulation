/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_BALL_H
#define NIMBLE_BALL_H

#define NL_MAX_PLAYERS (16)
#define NL_MAX_PARTICIPANTS (32)

#include <clog/clog.h>
#include <stdbool.h>
#include <stddef.h>

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
    int uniqueId;
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

typedef struct NlPosition {
    int x;
    int y;
} NlPosition;

typedef struct NlRect {
    int left;
    int right;
    int top;
    int bottom;
} NlRect;

typedef struct NlPlayer {
    int preferredTeamId;
    int controllingAvatarIndex;
    int assignedToParticipantIndex;
} NlPlayer;

typedef struct NlPlayers {
    NlPlayer players[NL_MAX_PLAYERS];
    size_t playerCount;
} NlPlayers;

typedef struct NlAvatar {
    NlPosition position;
    size_t controlledByPlayerIndex;
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
    NlRect rect;
    int halfLineX;
} NlArena;

typedef struct NlGame {
    NlParticipant participantLookup[NL_MAX_PARTICIPANTS];
    size_t lastParticipantLookupCount;
    NlPlayers players;
    NlAvatars avatars;
    NlGamePhase phase;
    NlArena arena;
    int countDown;
} NlGame;

void nlGameInit(NlGame* self);
void nlGameTick(NlGame* self, const NlPlayerInput* inputs, size_t inputCount, Clog* log);

#endif
