/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <basal/line_segment.h>
#include <basal/math.h>
#include <nimble-ball-simulation/nimble_ball_simulation.h>

static const float goalSize = 90;
static const float goalDetectWidth = 40;

static const float screenWidth = 640;

static const float spacing = 6.0f;
static const float arenaLeft = goalDetectWidth + spacing;
static const float arenaWidth = screenWidth - arenaLeft - spacing;
static const float arenaHeight = 280;

static const float arenaLineBottom = 20;
static const float arenaLineTop = arenaLineBottom + arenaHeight;
static const float arenaHeightMiddle = arenaLineBottom + arenaHeight / 2;
static const float arenaRight = arenaLeft + arenaWidth - 1;

const NlConstants g_nlConstants = {
    // Goal 0
    0, arenaLeft - goalDetectWidth, arenaHeightMiddle - goalSize / 2, goalDetectWidth, goalSize, false,

    // Goal 1
    1, arenaRight - goalDetectWidth, arenaHeightMiddle - goalSize / 2, goalDetectWidth, goalSize, true,

    arenaLeft, arenaLineBottom, arenaRight - goalDetectWidth,
    arenaLineBottom, // lower line segment

    arenaLeft, arenaLineTop, arenaRight - goalDetectWidth,
    arenaLineTop, // upper line segment

    arenaLeft, arenaLineTop, arenaLeft,
    arenaHeightMiddle + goalSize / 2, // upper left

    arenaLeft, arenaLineBottom, arenaLeft,
    arenaHeightMiddle - goalSize / 2, // lower left

    arenaRight - goalDetectWidth, arenaLineTop, arenaRight - goalDetectWidth,
    arenaHeightMiddle + goalSize / 2, // upper right

    arenaRight - goalDetectWidth, arenaLineBottom, arenaRight - goalDetectWidth,
    arenaHeightMiddle - goalSize / 2, //

    (int) (62.5f * 60.0f), // matchDuration
};

#define SLIDE_TACKLE_DURATION (20)

static void resetBallToMiddlePosition(NlBall* ball)
{
    ball->circle.center.x = arenaWidth / 2;
    ball->circle.center.y = arenaHeight / 2;
    ball->velocity = blVector2Zero();
    ball->collideCounter = 0;
}

void nlGameInit(NlGame* self)
{
    self->phase = NlGamePhaseWaitingForPlayers;
    self->players.playerCount = 0;
    self->avatars.avatarCount = 0;

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        self->participantLookup[i].isUsed = false;
    }
    self->lastParticipantLookupCount = 0;

    self->ball.circle.radius = 10.0f;
    resetBallToMiddlePosition(&self->ball);

    self->ball.velocity.x = 2.0f;
    self->ball.velocity.y = 1.6f;

    self->teams.teamCount = 2;
    self->teams.teams[0].score = 0;
    self->teams.teams[1].score = 1;

    self->tickCount = 0;
    self->matchClockLeftInTicks = g_nlConstants.matchDurationInTicks;
    self->latestScoredTeamIndex = 0xff;
}

static NlAvatar* spawnAvatarForPlayer(NlAvatars* self, NlPlayer* player, BlVector2 spawnPosition)
{
    if (player->preferredTeamId != 0 && player->preferredTeamId != 1) {
        CLOG_ERROR("test")
    }
    size_t avatarIndex = self->avatarCount++;
    CLOG_ASSERT(self->avatarCount < 3, "Wrong avatar count")

    NlAvatar* avatar = &self->avatars[avatarIndex];
    avatar->avatarIndex = (uint8_t) avatarIndex;
    avatar->circle.center = spawnPosition;
    avatar->circle.radius = 20.0f;
    avatar->controlledByPlayerIndex = player->playerIndex;
    avatar->kickCooldown = 0u;
    avatar->dribbleCooldown = 0u;
    avatar->velocity.x = 0;
    avatar->velocity.y = 0;
    avatar->requestedVelocity.x = 0;
    avatar->requestedVelocity.y = 0;
    avatar->teamIndex = player->preferredTeamId;

    avatar->slideTackleRemainingTicks = 0u;
    avatar->slideTackleCooldown = 0u;
    avatar->slideTackleRotation = 0;
    avatar->requestSlideTackle = false;
    avatar->kickedCounter = 0u;
    avatar->visualRotation = 0;

    player->controllingAvatarIndex = (uint8_t) avatarIndex;

    return avatar;
}

static void spawnAvatarsForPlayers(NlGame* self, Clog* log)
{
    for (size_t playerIndex = 0u; playerIndex < self->players.playerCount; ++playerIndex) {
        NlPlayer* player = &self->players.players[playerIndex];
        if (player->preferredTeamId == NL_TEAM_UNDEFINED) {
            continue;
        }

        BlVector2 spawnPosition = {player->preferredTeamId == 1 ? arenaWidth - goalDetectWidth - 20.0f
                                                                : goalDetectWidth + 40.0f,
                                   (float) ((float) playerIndex * 40.0f + goalDetectWidth + 20.0f)};

        NlAvatar* avatar = spawnAvatarForPlayer(&self->avatars, player, spawnPosition);
#if defined CLOG_LOG_ENABLED
        CLOG_C_DEBUG(log, "spawning avatar %hhu for player %zu (participant %d)", avatar->avatarIndex, playerIndex,
                     player->assignedToParticipantIndex)
#else
        (void) log;
        (void) avatar;
#endif
        player->phase = NlPlayerPhasePlaying;
    }
}

static int collideAgainstBorders(BlCircle* circle, BlVector2* velocity, float* biggestDepth, float safeDistance,
                                 float dampening)
{
    BlCircle circleCheck = *circle;
    circleCheck.radius = circle->radius + safeDistance;
    *biggestDepth = 0;

    int collisionCount = 0;

    for (size_t i = 0; i < sizeof(g_nlConstants.borderSegments) / sizeof(g_nlConstants.borderSegments[0]); ++i) {
        BlLineSegment lineSegmentToCheck = g_nlConstants.borderSegments[i];
        BlCollision collision = blLineSegmentCircleIntersect(lineSegmentToCheck, circleCheck);
        if (collision.depth > 0) {
            float reflectDot = blVector2Dot(*velocity, collision.normal);
            *velocity = blVector2Scale(blVector2Reflect(*velocity, collision.normal), dampening);
            circle->center = blVector2AddScale(circle->center, collision.normal, collision.depth + 0.1f);
            float impact = blFabs(reflectDot);
            if (impact > *biggestDepth) {
                *biggestDepth = impact;
            }
            collisionCount++;
        }
    }

    return collisionCount;
}

static bool atLeastOnePlayerHasCommittedToATeam(const NlPlayers* players)
{
    for (size_t i = 0; i < players->playerCount; ++i) {
        const NlPlayer* player = &players->players[i];
        if (player->phase == NlPlayerPhaseCommittedToTeam) {
            return true;
        }
    }

    return false;
}

static void tickWaitingForPlayers(NlGame* self, Clog* log)
{
    if (self->players.playerCount > 0 && atLeastOnePlayerHasCommittedToATeam(&self->players)) {
        CLOG_C_DEBUG(log, "start count down")
        self->phase = NlGamePhaseCountDown;
        self->phaseCountDown = 62 * 3;
        spawnAvatarsForPlayers(self, log);
    }
}

static void removePlayer(NlParticipant* participants, NlPlayers* self, size_t indexToRemove)
{
    self->players[indexToRemove] = self->players[--self->playerCount];
    participants[self->players[indexToRemove].assignedToParticipantIndex].playerIndex = (uint8_t) indexToRemove;
}

static void despawnAvatar(NlPlayers* players, NlAvatars* avatars, size_t indexToRemove)
{
    CLOG_ASSERT(indexToRemove < avatars->avatarCount, "avatar index is corrupt")
    NlAvatar* avatarToRemove = &avatars->avatars[indexToRemove];
    if (avatarToRemove->controlledByPlayerIndex != 0xff) {
        players->players[avatarToRemove->controlledByPlayerIndex].controllingAvatarIndex = 0xff;
    }

    avatars->avatars[indexToRemove] = avatars->avatars[--avatars->avatarCount];
}

static NlPlayer* spawnPlayer(NlPlayers* players, uint8_t participantId)
{
    size_t playerIndex = players->playerCount++;
    NlPlayer* assignedPlayer = &players->players[playerIndex];
    assignedPlayer->playerIndex = (uint8_t) playerIndex;
    assignedPlayer->assignedToParticipantIndex = participantId;
    assignedPlayer->controllingAvatarIndex = NL_AVATAR_INDEX_UNDEFINED;
    assignedPlayer->preferredTeamId = NL_TEAM_UNDEFINED;

    return assignedPlayer;
}

const NlPlayer* nlGameFindSimulationPlayerFromParticipantId(const NlGame* self, uint8_t participantId)
{
    for (size_t i = 0; i < self->players.playerCount; ++i) {
        const NlPlayer* simulationPlayer = &self->players.players[i];
        if (simulationPlayer->assignedToParticipantIndex == participantId) {
            return simulationPlayer;
        }
    }
    return 0;
}

static NlPlayer* participantJoined(NlPlayers* players, NlParticipant* participant, Clog* log)
{
    (void) log;

    NlPlayer* player = spawnPlayer(players, participant->participantId);
    CLOG_C_INFO(log, "participant has joined. player count is %hhu. created player %d for participant %d",
                players->playerCount, player->playerIndex, participant->participantId)

    participant->playerIndex = player->playerIndex;
    participant->isUsed = true;

    return player;
}

static BlVector2 findGoodSpawnPosition(NlPlayer* player)
{
    BlVector2 spawnPosition = {player->preferredTeamId == 1 ? arenaWidth - goalDetectWidth - 20.0f
                                                            : goalDetectWidth + 40.0f,
                               (float) ((float) player->playerIndex * 40.0f + goalDetectWidth + 20.0f)};
    return spawnPosition;
}

static void gameRulesForJoiningPlayer(NlGame* game, NlPlayer* player)
{
    if (game->phase == NlGamePhaseWaitingForPlayers) {
        player->preferredTeamId = 0xffU;
        player->phase = NlPlayerPhaseSelectTeam;
        // It will be spawned later
        return;
    }

    player->phase = NlPlayerPhaseSelectTeam;
}

static void participantLeft(NlPlayers* players, NlAvatars* avatars, NlParticipant* participants,
                            NlParticipant* participant, Clog* log)
{
    (void) log;
    NlPlayer* assignedPlayer = &players->players[participant->playerIndex];
    int assignedAvatarIndex = assignedPlayer->controllingAvatarIndex;
    if (assignedAvatarIndex != NL_AVATAR_INDEX_UNDEFINED) {
        despawnAvatar(players, avatars, (size_t) assignedAvatarIndex);
    }

    removePlayer(participants, players, participant->playerIndex);

    CLOG_C_INFO(log, "someone has left releasing player %hhu previously assigned to participant %d",
                participant->playerIndex, participant->participantId)

    participant->isUsed = false;
}

static void spawnAtFreePosition(NlGame* game, NlPlayer* player)
{
    BlVector2 spawnPosition = findGoodSpawnPosition(player);

    spawnAvatarForPlayer(&game->avatars, player, spawnPosition);
}

static void checkInputDiff(NlGame* self, const NlPlayerInputWithParticipantInfo* inputs, size_t inputCount, Clog* log)
{
    if (inputCount != self->lastParticipantLookupCount) {
        CLOG_C_INFO(log, "a participant has either been added or removed, count is different. was %hhu and is now %zu",
                    self->lastParticipantLookupCount, inputCount)
    }

    for (size_t i = 0; i < NL_MAX_PARTICIPANTS; ++i) {
        self->participantLookup[i].internalMarked = false;
    }

    for (size_t i = 0; i < inputCount; ++i) {
        NlParticipant* participant = &self->participantLookup[inputs[i].participantId];
        if (!participant->isUsed) {
            participant->isUsed = true;
            participant->participantId = inputs[i].participantId;
            NlPlayer* player = participantJoined(&self->players, participant, log);
            gameRulesForJoiningPlayer(self, player);
        }
        if (participant->playerIndex != 0xff) {
            self->players.players[participant->playerIndex].playerInput = inputs[i].playerInput;
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

    self->lastParticipantLookupCount = (uint8_t) inputCount;
}

static void tickCountDown(NlGame* self)
{
    if (self->phaseCountDown == 0) {
        self->phase = NlGamePhasePlaying;
        return;
    }
    self->phaseCountDown--;
}

#define MINIMAL_VELOCITY (0.1f)

static void tickBall(NlBall* ball)
{
    ball->velocity = blVector2Scale(ball->velocity, 0.988f);

    ball->circle.center = blVector2Add(ball->circle.center, ball->velocity);

    float biggestDepth;
    int collided = collideAgainstBorders(&ball->circle, &ball->velocity, &biggestDepth, 0.f, 0.91f);
    if (collided > 0) {
        if (biggestDepth > 0.8f && blVector2Length(ball->velocity) > 0.7f) {
            ball->collideCounter++;
        }
    }
    if (blVector2SquareLength(ball->velocity) < MINIMAL_VELOCITY) {
        ball->velocity = blVector2Zero();
    }
}
static bool isAllowedToJoinWithAvatar(NlGamePhase gamePhase)
{
    return gamePhase == NlGamePhaseCountDown || gamePhase == NlGamePhaseAfterAGoal;
}

static void playerToAvatarControl(NlGame* game, NlPlayers* players, NlAvatars* avatars)
{
    for (size_t i = 0; i < players->playerCount; ++i) {
        NlPlayer* player = &players->players[i];

        switch (player->playerInput.inputType) {
            case NlPlayerInputTypeInGame: {
                player->isWaitingForReconnect = false;
                if (player->controllingAvatarIndex == NL_AVATAR_INDEX_UNDEFINED) {
                    continue;
                }
                const NlPlayerInGameInput* inGameInput = &player->playerInput.input.inGameInput;
                NlAvatar* avatar = &avatars->avatars[player->controllingAvatarIndex];
                BlVector2 requestVelocity;
                requestVelocity.x = inGameInput->horizontalAxis;
                requestVelocity.y = inGameInput->verticalAxis;
                avatar->requestedVelocity = blVector2Scale(requestVelocity, 0.4f);
                avatar->requestBuildKickPower = inGameInput->buttons & 0x01;
                avatar->requestSlideTackle = inGameInput->buttons & 0x02;

            } break;
            case NlPlayerInputTypeSelectTeam: {
                if (player->phase == NlPlayerPhaseSelectTeam) {
                    NlPlayerSelectTeam* selectTeamInput = &player->playerInput.input.selectTeam;
                    CLOG_INFO("player selected team %d", selectTeamInput->preferredTeamToJoin)

                    if (player->preferredTeamId != selectTeamInput->preferredTeamToJoin) {
                        CLOG_NOTICE("player made a choice %d", selectTeamInput->preferredTeamToJoin)
                    }
                    player->preferredTeamId = selectTeamInput->preferredTeamToJoin;
                    player->phase = NlPlayerPhaseCommittedToTeam;
                    if (player->controllingAvatarIndex == NL_AVATAR_INDEX_UNDEFINED &&
                        isAllowedToJoinWithAvatar(game->phase) && player->preferredTeamId != NL_TEAM_UNDEFINED) {
                        spawnAtFreePosition(game, player);
                        player->phase = NlPlayerPhasePlaying;
                    }
                } else {
                }
                break;
            }
            case NlPlayerInputTypeForced:
                // Do nothing
                break;
            case NlPlayerInputTypeWaitingForReconnect:
                CLOG_INFO("waiting for reconnect!")
                player->isWaitingForReconnect = true;
                break;
        }
    }
}

static void tickAvatars(NlAvatars* avatars)
{
    for (size_t i = 0; i < avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];

        if (avatar->slideTackleRemainingTicks > 0) {
            BlVector2 slideUnitDirection = blVector2FromAngle(avatar->slideTackleRotation);
            float normalizedDuration = (float) avatar->slideTackleRemainingTicks / (float) SLIDE_TACKLE_DURATION;
            float slideFactor = normalizedDuration * normalizedDuration * 8.0f;
            avatar->velocity = blVector2AddScale(avatar->velocity, slideUnitDirection, slideFactor);
        } else if (avatar->slideTackleCooldown > 0) {
            avatar->velocity = blVector2Zero();
        } else {
            float speedFactor = avatar->kickPower > 0 ? 0.05f : 0.2f;
            avatar->velocity = blVector2AddScale(avatar->velocity, avatar->requestedVelocity, speedFactor);
        }

        const float maxAvatarSpeed = 60.0f;
        if (blVector2SquareLength(avatar->velocity) > maxAvatarSpeed * maxAvatarSpeed) {
            avatar->velocity = blVector2Scale(blVector2Unit(avatar->velocity), maxAvatarSpeed);
        }
        avatar->velocity = blVector2Scale(avatar->velocity, 0.98f);
        avatar->circle.center = blVector2Add(avatar->circle.center, avatar->velocity);
        float length = blVector2SquareLength(avatar->requestedVelocity);
        if (length > 0.001f) {
            float target = blVector2ToAngle(avatar->requestedVelocity);
            float angleDiff = blAngleMinimalDiff(target, avatar->visualRotation);
            avatar->visualRotation += angleDiff * 0.1f;
        }

        float biggestDepth;
        collideAgainstBorders(&avatar->circle, &avatar->velocity, &biggestDepth, 10.0f, 0);
    }
}

#define DRIBBLE_REACH_EXTRA (-2.0f)
#define DRIBBLE_DISTANCE_FROM_BODY (10.0f)

static void tickDribble(NlAvatars* avatars, NlBall* ball)
{
    for (size_t i = 0; i < avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        if (avatar->dribbleCooldown > 0) {
            avatar->dribbleCooldown--;
            continue;
        }
        BlCircle dribbleReach = avatar->circle;
        dribbleReach.radius = avatar->circle.radius + DRIBBLE_REACH_EXTRA;
        if (blCircleOverlap(dribbleReach, ball->circle)) {
            BlVector2 avatarDirection = blVector2FromAngle(avatar->visualRotation);
            BlVector2 targetDribblePosition = blVector2AddScale(avatar->circle.center, avatarDirection,
                                                                DRIBBLE_DISTANCE_FROM_BODY);
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
    float normalizedKickPower = (float) kickPowerTicks / MAX_KICK_POWER_TICKS;
    BlVector2 kickVelocity = blVector2Scale(avatarDirection, normalizedKickPower * 10.0f + 1.0f);
    ball->velocity = blVector2Add(avatar->velocity, kickVelocity);
    float biggestDepth;
    collideAgainstBorders(&ball->circle, &ball->velocity, &biggestDepth, 0.f, 0.9f);
    avatar->kickCooldown = 14;
    avatar->dribbleCooldown = 12;
    avatar->kickedCounter++;
}

static bool checkGoal(const NlGoal* goal, const NlBall* ball, NlTeams* teams, uint8_t* latestTeamToScore)
{
    BlCollision collision = blRectCircleIntersect(goal->rect, ball->circle);
    if (fabsf(collision.depth) < 0.001f) {
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

    int opposingTeam = goal->ownedByTeam == 0 ? 1 : 0;
    teams->teams[opposingTeam].score++;
    *latestTeamToScore = (uint8_t) opposingTeam;
    return true;
}

static bool checkGoals(const NlGoal* goals, size_t goalCount, const NlBall* ball, NlTeams* teams,
                       uint8_t* latestScoredTeamIndex)
{
    bool someoneScored = false;
    for (size_t i = 0; i < goalCount; ++i) {
        const NlGoal* goal = &goals[i];
        someoneScored |= checkGoal(goal, ball, teams, latestScoredTeamIndex);
    }

    return someoneScored;
}

static void tickGoalCheck(NlTeams* teams, NlBall* ball, uint8_t* phase, uint16_t* phaseCountDown,
                          uint8_t* latestScoredTeamIndex)
{
    bool someoneScored = checkGoals(g_nlConstants.goals, 2, ball, teams, latestScoredTeamIndex);
    if (!someoneScored) {
        return;
    }

    *phase = NlGamePhaseAfterAGoal;
    *phaseCountDown = 62 * 4;
}

static void tickKick(NlAvatars* avatars, NlBall* ball)
{
    for (size_t i = 0; i < avatars->avatarCount; ++i) {
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

static void tickSlideTackle(NlAvatars* avatars)
{
    for (size_t i = 0; i < avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        if (avatar->slideTackleRemainingTicks > 0) {
            avatar->slideTackleRemainingTicks--;
            continue;
        }
        if (avatar->slideTackleCooldown > 0) {
            avatar->slideTackleCooldown--;
            continue;
        }
        if (avatar->requestSlideTackle) {
            avatar->slideTackleCooldown = 60;
            avatar->slideTackleRemainingTicks = SLIDE_TACKLE_DURATION;
            avatar->slideTackleRotation = avatar->visualRotation;
        }
    }
}

static void checkEndOfMatchTime(NlGame* self)
{
    if (self->matchClockLeftInTicks > 0) {
        self->matchClockLeftInTicks--;
        return;
    }

    self->phase = NlGamePhasePostGame;
    self->phaseCountDown = 62 * 6;
}

static void tickPlaying(NlGame* self)
{
    checkEndOfMatchTime(self);
    tickAvatars(&self->avatars);
    tickDribble(&self->avatars, &self->ball);
    tickKick(&self->avatars, &self->ball);
    tickSlideTackle(&self->avatars);
    tickBall(&self->ball);
    tickGoalCheck(&self->teams, &self->ball, &self->phase, &self->phaseCountDown, &self->latestScoredTeamIndex);
}

static void resetAvatarsToStartPositions(NlAvatars* avatars)
{
    int placedInEachTeam[2] = {0, 0};
    const int avatarCountInEachRow = 4;

    for (size_t i = 0; i < avatars->avatarCount; ++i) {
        NlAvatar* avatar = &avatars->avatars[i];
        int placed = placedInEachTeam[avatar->teamIndex]++;

        int rowIndex = placed / avatarCountInEachRow;
        int colIndex = placed % avatarCountInEachRow;

        int columnFactor = avatar->teamIndex ? 1 : -1;

        int column = (20 + colIndex * 40) * columnFactor;
        int row = rowIndex * 40;

        const int centerLine = (int) (arenaWidth / 2);
        column = centerLine + column;

        const int rowOffset = 50;
        float rotation = avatar->teamIndex ? -BL_PI : 0.f;
        avatar->visualRotation = rotation;
        avatar->circle.center.x = (float) column;
        avatar->circle.center.y = (float) (row + rowOffset);
        avatar->velocity = blVector2Zero();
        avatar->dribbleCooldown = 0;
        avatar->kickCooldown = 0;
        avatar->kickPower = 0;
        avatar->requestedVelocity = blVector2Zero();
        avatar->requestBuildKickPower = false;
        avatar->slideTackleRemainingTicks = 0;
        avatar->slideTackleCooldown = 0;
        avatar->slideTackleRotation = 0;
        avatar->requestSlideTackle = false;
    }
}

static void spawnAvatarsForWaitingPlayers(NlGame* self)
{
    for (size_t i = 0; i < self->players.playerCount; ++i) {
        for (size_t playerIndex = 0u; playerIndex < self->players.playerCount; ++playerIndex) {
            NlPlayer* player = &self->players.players[playerIndex];
            if (player->phase == NlPlayerPhaseCommittedToTeam &&
                player->controllingAvatarIndex == NL_AVATAR_INDEX_UNDEFINED &&
                player->preferredTeamId != NL_TEAM_UNDEFINED) {
                spawnAtFreePosition(self, player);
                player->phase = NlPlayerPhasePlaying;
            }
        }
    }
}

static void resetPitchAndStartCountdown(NlGame* self)
{
    spawnAvatarsForWaitingPlayers(self);

    resetAvatarsToStartPositions(&self->avatars);
    resetBallToMiddlePosition(&self->ball);

    self->phaseCountDown = 62 * 3;
    self->phase = NlGamePhaseCountDown;
}

static void tickAfterGoal(NlGame* self)
{
    if (self->phaseCountDown > 0) {
        self->phaseCountDown--;
        return;
    }

    resetPitchAndStartCountdown(self);
}

static void resetForNewMatch(NlGame* self)
{
    for (size_t i = 0; i < self->teams.teamCount; ++i) {
        self->teams.teams[i].score = 0;
    }

    self->matchClockLeftInTicks = g_nlConstants.matchDurationInTicks;

    resetPitchAndStartCountdown(self);
}

static void tickPostGame(NlGame* self)
{
    if (self->phaseCountDown > 0) {
        self->phaseCountDown--;
        return;
    }

    resetForNewMatch(self);
}

void nlGameTick(NlGame* self, const NlPlayerInputWithParticipantInfo* inputs, size_t inputCount, Clog* log)
{
    checkInputDiff(self, inputs, inputCount, log);
    playerToAvatarControl(self, &self->players, &self->avatars);

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
        case NlGamePhaseAfterAGoal:
            tickAfterGoal(self);
            break;
        case NlGamePhasePostGame:
            tickPostGame(self);
            break;
    }
}
