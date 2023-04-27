/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-ball-simulation/nimble_ball_simulation.h>
#include <basal/basal_line_segment.h>

const float goalSize = 90;
const float goalDetectWidth = 40;
const float arenaWidth = 640;
const float arenaHeight = 360;

const NlConstants g_nlConstants = {
    0, 0, 180-goalSize/2, goalDetectWidth, goalSize, false,
    1, arenaWidth-goalDetectWidth, 180-goalSize/2, goalDetectWidth, goalSize, true,
    goalDetectWidth,0,arenaWidth-1-goalDetectWidth,0, // lower line segment
    goalDetectWidth,arenaHeight-1,arenaWidth-goalDetectWidth,arenaHeight-1, // upper line segment

    goalDetectWidth,arenaHeight-1,goalDetectWidth,arenaHeight/2+goalSize/2, // upper left
    goalDetectWidth,0,goalDetectWidth,arenaHeight/2-goalSize/2, // lower left

    arenaWidth-1-goalDetectWidth,arenaHeight-1,arenaWidth-1-goalDetectWidth,arenaHeight/2+goalSize/2, // upper right
    arenaWidth-1-goalDetectWidth,0,arenaWidth-1-goalDetectWidth,arenaHeight/2-goalSize/2, // lower right
};



void nlGameInit(NlGame* self)
{
    self->phase = NlGamePhaseWaitingForPlayers;
    self->players.playerCount = 0;
    self->avatars.avatarCount = 0;

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        self->participantLookup[i].isUsed = false;
    }
    self->lastParticipantLookupCount = 0;

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
        avatar->circle.center.x = player->preferredTeamId == 1 ? arenaWidth - goalDetectWidth - 20.0f : goalDetectWidth + 40.0f;
        avatar->circle.center.y = i * 40.0 + goalDetectWidth + 20.0f;
        avatar->circle.radius = 20.0f;
        avatar->controlledByPlayerIndex = i;
        avatar->kickCooldown = 0;
        avatar->dribbleCooldown = 0;
        avatar->velocity.x = 0;
        avatar->velocity.y = 0;
        avatar->requestedVelocity.x = 0;
        avatar->requestedVelocity.y = 0;
        avatar->teamIndex = player->preferredTeamId;

        player->controllingAvatarIndex = avatarIndex;

        CLOG_C_DEBUG(log, "spawning avatar %zu for player %zu (participant %d)", avatarIndex, i,
                     player->assignedToParticipantIndex)
    }
}

static void collideAgainstBorders(BlCircle* circle, BlVector2* velocity, float safeDistance, float dampening)
{
    BlCircle circleCheck = *circle;
    circleCheck.radius = circle->radius + safeDistance;
    for (size_t i=0 ; i<sizeof(g_nlConstants.borderSegments) / sizeof(g_nlConstants.borderSegments[0]); ++i) {
        BlLineSegment lineSegmentToCheck = g_nlConstants.borderSegments[i];
        BlCollision collision = blLineSegmentCircleIntersect(lineSegmentToCheck, circleCheck);
        if (collision.depth > 0) {
            *velocity = blVector2Scale(blVector2Reflect(*velocity, collision.normal), dampening);
            circle->center = blVector2AddScale(circle->center, collision.normal, collision.depth);
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
    assignedPlayer->preferredTeamId = players->playerCount == 1 ? 0 : 1;

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

#define MINIMAL_VELOCITY (0.12f)

static void tickBall(NlBall* ball)
{
    ball->velocity = blVector2Scale(ball->velocity, 0.995f);

    ball->circle.center = blVector2Add(ball->circle.center, ball->velocity);

    collideAgainstBorders(&ball->circle, &ball->velocity, 0.f, 0.9f);
    if (blVector2SquareLength(ball->velocity) < MINIMAL_VELOCITY) {
        ball->velocity = blVector2Zero();
    }
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
        BlVector2 requestVelocity;
        requestVelocity.x = inGameInput->horizontalAxis;
        requestVelocity.y = inGameInput->verticalAxis;
        avatar->requestedVelocity = blVector2Scale(requestVelocity, 0.4f);
        avatar->requestBuildKickPower = inGameInput->passButton;
    }

}



static void tickAvatars(NlAvatars* avatars)
{
    for (size_t i=0; i<avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];

        float speedFactor = avatar->kickPower > 0 ? 0.2f : 1.0f;
        avatar->velocity = blVector2AddScale(avatar->velocity, avatar->requestedVelocity, speedFactor);

        const float maxAvatarSpeed = 50.0f;
        if (blVector2SquareLength(avatar->velocity) > maxAvatarSpeed*maxAvatarSpeed) {
            avatar->velocity = blVector2Scale(blVector2Unit(avatar->velocity),  maxAvatarSpeed);
        }
        avatar->velocity = blVector2Scale(avatar->velocity,  0.9f);
        avatar->circle.center = blVector2Add(avatar->circle.center, avatar->velocity);
        float length = blVector2SquareLength(avatar->requestedVelocity);
        if (length > 0.001f) {
            float target = blVector2ToAngle(avatar->requestedVelocity);
            float angleDiff = blAngleMinimalDiff(target, avatar->visualRotation);
            avatar->visualRotation += angleDiff * 0.1f;
        }

        collideAgainstBorders(&avatar->circle, &avatar->velocity, 10.0f, 0);
    }
}

#define DRIBBLE_REACH_EXTRA (-2.0f)
#define DRIBBLE_DISTANCE_FROM_BODY (10.0f)

static void tickDribble(NlAvatars* avatars, NlBall* ball)
{
    for (size_t i=0; i<avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        if (avatar->dribbleCooldown > 0) {
            avatar->dribbleCooldown--;
            continue;
        }
        BlCircle dribbleReach = avatar->circle;
        dribbleReach.radius = avatar->circle.radius + DRIBBLE_REACH_EXTRA;
        if (blCircleOverlap(dribbleReach, ball->circle)) {
            BlVector2 avatarDirection = blVector2FromAngle(avatar->visualRotation);
            BlVector2 targetDribblePosition = blVector2AddScale(avatar->circle.center, avatarDirection, DRIBBLE_DISTANCE_FROM_BODY);
            BlVector2 diffFromTargetDribblePosition = blVector2Sub(targetDribblePosition, ball->circle.center);
            ball->circle.center = blVector2AddScale(ball->circle.center, diffFromTargetDribblePosition, 0.2f);
            ball->velocity = blVector2Add(avatar->velocity, blVector2Scale(avatarDirection, 2.0f));
        }
    }
}

#define MAX_KICK_POWER_TICKS (100)

static void performKick(NlAvatar* avatar, NlBall* ball, uint8_t kickPowerTicks)
{
    BlCircle increasedReach = avatar->circle;
    increasedReach.radius = avatar->circle.radius * 2.0f;

    if (!blCircleOverlap(increasedReach, ball->circle)) {
        // Ball was not close, the avatar kicked air
        return;
    }
    BlVector2 avatarDirection = blVector2FromAngle(avatar->visualRotation);
    float normalizedKickPower = (float)kickPowerTicks / MAX_KICK_POWER_TICKS;
    BlVector2 kickVelocity = blVector2Scale(avatarDirection, normalizedKickPower * 10.0f + 1.0f);
    ball->velocity = blVector2Add(avatar->velocity, kickVelocity);
    collideAgainstBorders(&ball->circle, &ball->velocity, 0.f, 0.9f);
    avatar->kickCooldown = 14;
    avatar->dribbleCooldown = 12;
}

static bool checkGoal(const NlGoal* goal, const NlBall* ball, NlTeams* teams)
{
    BlCollision collision = blRectCircleIntersect(goal->rect, ball->circle);
    if (collision.depth == 0) {
        return false;
    }

    bool isGoal = false;
    if (goal->facingLeft) {
        isGoal = (ball->circle.center.x - ball->circle.radius > goal->rect.position.x);
    } else {
        isGoal = (ball->circle.center.x + ball->circle.radius < goal->rect.position.x + goal->rect.size.x);
    }

    if (!isGoal) {
        return false;
    }

    CLOG_VERBOSE("GOAL! for %d", goal->ownedByTeam)
    teams->teams[goal->ownedByTeam].score++;
    return true;
}

static bool checkGoals(const NlGoal* goals, size_t goalCount, const NlBall* ball, NlTeams* teams)
{
    bool someoneScored = false;
    for (size_t i =0 ; i< goalCount; ++i) {
        const NlGoal* goal = &goals[i];
        someoneScored |= checkGoal(goal, ball, teams);
    }

    return someoneScored;
}

static void tickGoalCheck(NlTeams* teams, NlBall* ball, NlGamePhase* phase)
{
    bool someoneScored = checkGoals(g_nlConstants.goals, 2, ball, teams);
    if (someoneScored) {
        *phase = NlGamePhasePostGame;
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
        if (avatar->requestBuildKickPower) {
            if (avatar->kickPower < MAX_KICK_POWER_TICKS) {
                avatar->kickPower++;
            }
        } else {
            // Button is released, use all the built-up kick power
            if (avatar->kickPower > 0) {
                performKick(avatar, ball, avatar->kickPower);
                avatar->kickPower = 0;
            }
            continue;
        }
    }
}

static void tickPlaying(NlGame* self)
{
    tickAvatars(&self->avatars);
    tickDribble(&self->avatars, &self->ball);
    tickKick(&self->avatars, &self->ball);
    tickBall(&self->ball);
    tickGoalCheck(&self->teams,  &self->ball, &self->phase);
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
