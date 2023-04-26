/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-ball-simulation/nimble_ball_simulation.h>
#include <basal/basal_line_segment.h>
#include <basal/basal_math.h>

void nlGameInit(NlGame* self)
{
    self->phase = NlGamePhaseWaitingForPlayers;
    self->players.playerCount = 0;
    self->avatars.avatarCount = 0;

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        self->participantLookup[i].isUsed = false;
    }
    self->lastParticipantLookupCount = 0;

    self->arena.rect.vector.x = 0;
    self->arena.rect.vector.y = 200;
    self->arena.rect.size.width = 0;
    self->arena.rect.size.height = 100;
    self->arena.halfLineX = 100;

    self->ball.circle.center.x = 100.0f;
    self->ball.circle.center.y = 50.0f;
    self->ball.circle.radius = 10.0f;

    self->ball.velocity.x = 2.0f;
    self->ball.velocity.y = 4.0f;

    self->tickCount = 0;
}

static void spawnAvatarsForPlayers(NlGame* self, Clog* log)
{
    for (size_t i = 0; i < self->players.playerCount; ++i) {
        NlPlayer* player = &self->players.players[i];
        size_t avatarIndex = self->avatars.avatarCount++;
        NlAvatar* avatar = &self->avatars.avatars[avatarIndex];
        avatar->circle.center.x = player->preferredTeamId == 1 ? 75 : 25;
        avatar->circle.center.y = i * 10.0f + 20.0f;
        avatar->circle.radius = 15.0f;
        avatar->controlledByPlayerIndex = i;
        avatar->kickCooldown = 0;
        avatar->velocity.x = 0;
        avatar->velocity.y = 0;
        avatar->requestedVelocity.x = 0;
        avatar->requestedVelocity.y = 0;

        player->controllingAvatarIndex = avatarIndex;

        CLOG_C_DEBUG(log, "spawning avatar %zu for player %zu (participant %d)", avatarIndex, i,
                     player->assignedToParticipantIndex)
    }
}

static void collideBallAgainstBorders(NlBall* ball)
{
    const float arenaWidth = 640.0f;
    const float arenaHeight = 360.0f;

    bl_line_segment lineSegment;
    lineSegment.a.x = 0;
    lineSegment.a.y = 0;
    lineSegment.b.x = arenaWidth;
    lineSegment.b.y = 0;

    bl_line_segment lineSegmentLower;
    lineSegmentLower.a.x = 0;
    lineSegmentLower.a.y = arenaHeight;
    lineSegmentLower.b.x = arenaWidth;
    lineSegmentLower.b.y = arenaHeight;

    bl_line_segment lineSegmentRight;
    lineSegmentRight.a.x = arenaWidth;
    lineSegmentRight.a.y = arenaHeight;
    lineSegmentRight.b.x = arenaWidth;
    lineSegmentRight.b.y = 0;

    bl_line_segment lineSegmentLeft;
    lineSegmentLeft.a.x = 0;
    lineSegmentLeft.a.y = arenaHeight;
    lineSegmentLeft.b.x = 0;
    lineSegmentLeft.b.y = 0;

    bl_line_segment lineSegments[4];

    lineSegments[0] = lineSegment;
    lineSegments[1] = lineSegmentLower;
    lineSegments[2] = lineSegmentRight;
    lineSegments[3] = lineSegmentLeft;

    for (size_t i=0 ; i<sizeof(lineSegments) / sizeof(lineSegments[0]); ++i) {
        bl_line_segment lineSegmentToCheck = lineSegments[i];
        bl_collision collision = bl_line_segment_circle_intersect(lineSegmentToCheck, ball->circle);
        if (collision.depth > 0) {
            ball->velocity = bl_vector2_reflect(ball->velocity, collision.normal);
            ball->circle.center = bl_vector2_add_scale(ball->circle.center, collision.normal, collision.depth);
        }
    }
}

static void tickWaitingForPlayers(NlGame* self, Clog* log)
{
    if (self->players.playerCount > 0) {
        CLOG_C_DEBUG(log, "start count down")
        self->phase = NlGamePhaseCountDown;
        spawnAvatarsForPlayers(self, log);

        self->countDown = 62 * 3;
    }
}

static void removePlayer(NlParticipant* participants, NlPlayers* self, size_t indexToRemove)
{
    self->players[indexToRemove] = self->players[--self->playerCount];
    participants[self->players[indexToRemove].assignedToParticipantIndex].playerIndex = indexToRemove;
}

static void despawnAvatar(NlPlayers* players, NlAvatars* avatars, size_t indexToRemove)
{
    NlAvatar* avatarToRemove = &avatars->avatars[indexToRemove];
    if (avatarToRemove->controlledByPlayerIndex >= 0) {
        players->players[avatarToRemove->controlledByPlayerIndex].controllingAvatarIndex = -1;
    }

    avatars->avatars[indexToRemove] = avatars->avatars[--avatars->avatarCount];
}

static void participantJoined(NlPlayers* players, NlParticipant* participant, Clog* log)
{
    size_t playerIndex = players->playerCount++;
    NlPlayer* assignedPlayer = &players->players[playerIndex];
    assignedPlayer->assignedToParticipantIndex = participant->participantId;
    assignedPlayer->controllingAvatarIndex = -1;
    assignedPlayer->preferredTeamId = -1;

    CLOG_C_DEBUG(log, "participant has joined. creating player %zu for participant %d", playerIndex,
                 participant->participantId)

    participant->playerIndex = playerIndex;
    participant->isUsed = true;
}

static void participantLeft(NlPlayers* players, NlAvatars* avatars, NlParticipant* participants,
                            NlParticipant* participant, Clog* log)
{
    NlPlayer* assignedPlayer = &players->players[participant->playerIndex];
    int assignedAvatarIndex = assignedPlayer->controllingAvatarIndex;
    if (assignedAvatarIndex >= 0) {
        despawnAvatar(players, avatars, assignedAvatarIndex);
    }

    removePlayer(participants, players, participant->playerIndex);

    CLOG_C_VERBOSE(log, "someone has left releasing player %zu previously assigned to participant %d",
                   participant->playerIndex, participant->participantId)

    participant->isUsed = false;
}

static void checkInputDiff(NlGame* self, const NlPlayerInput* inputs, size_t inputCount, Clog* log)
{
    if (inputCount != self->lastParticipantLookupCount) {
        CLOG_C_VERBOSE(log, "someone has either left or added, count is different %zu vs %zu", inputCount,
                       self->lastParticipantLookupCount)
    }

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        self->participantLookup[i].internalMarked = false;
    }

    for (size_t i = 0; i < inputCount; ++i) {
        NlParticipant* participant = &self->participantLookup[inputs[i].participantId];
        if (!participant->isUsed) {
            participant->isUsed = true;
            participant->participantId = inputs[i].participantId;
            participantJoined(&self->players, participant, log);
        }
        if (participant->playerIndex >= 0) {
            self->players.players[participant->playerIndex].playerInput = inputs[i];
        }
        participant->internalMarked = true;
    }

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        NlParticipant* participant = &self->participantLookup[i];
        if (participant->isUsed && !participant->internalMarked) {
            // An active participants that is no longer in the provided inputs must be removed
            participantLeft(&self->players, &self->avatars, self->participantLookup, participant, log);
        }
    }

    self->lastParticipantLookupCount = inputCount;
}

static void tickCountDown(NlGame* self)
{
    if (self->countDown == 0) {
        self->phase = NlGamePhasePlaying;
        return;
    }
    self->countDown--;
}

static void tickBall(NlBall* ball)
{
    ball->velocity = bl_vector2_mul_scalar(ball->velocity, 0.995f);

    ball->circle.center.x += ball->velocity.x;
    ball->circle.center.y += ball->velocity.y;

    collideBallAgainstBorders(ball);
}

static void playerToAvatarControl(NlPlayers* players, NlAvatars* avatars)
{
    for (size_t i=0; i<players->playerCount; ++i) {
        NlPlayer * player = &players->players[i];
        if (player->controllingAvatarIndex < 0) {
            continue;
        }
        if (player->playerInput.inputType != NlPlayerInputTypeInGame) {
            continue;
        }

        NlAvatar* avatar = &avatars->avatars[player->controllingAvatarIndex];
        const NlPlayerInGameInput* inGameInput = &player->playerInput.input.inGameInput;
        bl_vector2 requestVelocity;
        requestVelocity.x = inGameInput->horizontalAxis;
        requestVelocity.y = inGameInput->verticalAxis;
        avatar->requestedVelocity = bl_vector2_mul_scalar(requestVelocity, 0.2f);
        avatar->kickWhenAble = inGameInput->passButton;
    }

}



static void tickAvatars(NlAvatars* avatars)
{
    for (size_t i=0; i<avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];

        avatar->velocity = bl_vector2_add(avatar->velocity, avatar->requestedVelocity);
        const float maxAvatarSpeed = 6.0f;
        if (bl_vector2_square_length(avatar->velocity) > maxAvatarSpeed*maxAvatarSpeed) {
            avatar->velocity = bl_vector2_mul_scalar(bl_vector2_unit(avatar->velocity),  maxAvatarSpeed);
        }
        avatar->velocity = bl_vector2_mul_scalar(avatar->velocity,  0.9f);
        avatar->circle.center = bl_vector2_add(avatar->circle.center, avatar->velocity);
        float length = bl_vector2_square_length(avatar->requestedVelocity);
        if (length > 0.001f) {
            avatar->visualRotation = bl_math_atan2(avatar->requestedVelocity.y, avatar->requestedVelocity.x);
        }
    }
}

static void tickDribble(NlAvatars* avatars, NlBall* ball)
{
    for (size_t i=0; i<avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        if (bl_circle_overlap(avatar->circle, ball->circle)) {
            //float factor = bl_vector2_dot(ball->velocity, avatar->velocity);
            ball->velocity = bl_vector2_mul_scalar(avatar->velocity, 1.5f);
        }
    }
}

static void tickKick(NlAvatars* avatars, NlBall* ball)
{
    for (size_t i=0; i<avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        if (avatar->kickCooldown > 0) {
            avatar->kickCooldown--;
            continue;
        }
        if (!avatar->kickWhenAble) {
            continue;
        }
        bl_circle increasedReach = avatar->circle;
        increasedReach.radius = avatar->circle.radius * 2.5f;

        if (bl_circle_overlap(increasedReach, ball->circle)) {
            bl_vector2 direction;
            direction.x = bl_math_cos(avatar->visualRotation);
            direction.y = bl_math_sin(avatar->visualRotation);
            ball->velocity = bl_vector2_add_scale(ball->velocity, direction, 5.8f);
            avatar->kickCooldown = 14;
        }
    }
}

static void tickPlaying(NlGame* self)
{
    tickAvatars(&self->avatars);
    tickDribble(&self->avatars, &self->ball);
    tickKick(&self->avatars, &self->ball);
    tickBall(&self->ball);
}

void nlGameTick(NlGame* self, const NlPlayerInput* inputs, size_t inputCount, Clog* log)
{
    checkInputDiff(self, inputs, inputCount, log);
    playerToAvatarControl(&self->players, &self->avatars);

    self->tickCount++;
    switch (self->phase) {
        case NlGamePhaseWaitingForPlayers:
            tickWaitingForPlayers(self, log);
            break;
        case NlGamePhaseCountDown:
            tickCountDown(self);
            break;
        case NlGamePhasePlaying:
            tickPlaying(self);
            break;
        case NlGamePhasePostGame:
            break;
    }
}
