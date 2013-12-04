#include <time.h>

#include "bzfsAPI.h"
#include "bzToolkitAPI.h"

void resetPlayerScore(int playerID)
{
    bz_setPlayerWins(playerID, 0);
    bz_setPlayerLosses(playerID, 0);
}

int getLastTankStanding()
{
    int lastTankStanding = -1;

    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        if (bz_getPlayerByIndex(playerList->get(i))->spawned)
        {
            lastTankStanding = playerList->get(i);
        }
    }

    bz_deleteIntList(playerList);
    return lastTankStanding;
}

int getPlayerWithLowestScore()
{
    int playerWithLowestScore = -1;
    int lowestHighestScore = 9999;
    bool foundDuplicate = false;

    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        if (bz_getPlayerTeam(playerList->get(i)) == eObservers)
        {
            continue;
        }

        int playerScore = bz_getPlayerWins(playerList->get(i)) - bz_getPlayerLosses(playerList->get(i));

        if (playerScore < lowestHighestScore)
        {
            lowestHighestScore = playerScore;
            playerWithLowestScore = playerList->get(i);
            foundDuplicate = false;
        }
        else if (playerScore == lowestHighestScore)
        {
            foundDuplicate = true;
        }
    }

    if (foundDuplicate)
    {
        playerWithLowestScore = -1;
    }

    bz_deleteIntList(playerList);
    return playerWithLowestScore;
}

class lastTankStanding : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "Last Tank Standing";}
    virtual void Init(const char* config);
    virtual void Cleanup(void);
    virtual void Event(bz_EventData *eventData);

    virtual bool SlashCommand(int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    double
        startOfMatch,
        bzdb_gravity,
        bzdb_jumpVelocity,
        bzdb_reloadTime,
        bzdb_tankSpeed,
        bzdb_tankAngVel;

    bool
        isCountdownInProgress,
        isGameInProgress,
        firstRun;

    int
        countdownLength,
        kickTime,
        countdownProgress;

    time_t
        lastCountdownCheck,
        lastKickTime;
};

BZ_PLUGIN(lastTankStanding)

void lastTankStanding::Init(const char* commandLine)
{
    bz_debugMessage(4, "lastTankStanding plugin loaded");

    isCountdownInProgress = false;
    isGameInProgress = false;
    countdownLength = 15;
    startOfMatch = 0;
    kickTime = 60;

    Register(bz_eBZDBChange);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);

    bztk_registerCustomIntBZDB("_ltsKickTime", kickTime);
    bztk_registerCustomIntBZDB("_ltsCountdown", countdownLength);

    bz_registerCustomSlashCommand("start", this);
    bz_registerCustomSlashCommand("end", this);
}

void lastTankStanding::Cleanup(void)
{
    Flush();

    bz_removeCustomSlashCommand("start");
    bz_removeCustomSlashCommand("end");
}

void lastTankStanding::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eBZDBChange:
        {
            bz_BZDBChangeData_V1* bzdbChange = (bz_BZDBChangeData_V1*)eventData;

            if (bzdbChange->key == "_ltsKickTime")
            {
                if (atoi(bzdbChange->value.c_str()) > 0)
                {
                    kickTime = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    kickTime = 60;
                }
            }
            else if (bzdbChange->key == "_ltsCountdown")
            {
                if (atoi(bzdbChange->value.c_str()) > 0)
                {
                    countdownLength = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    countdownLength = 15;
                }
            }
        }

        case bz_ePlayerJoinEvent:
        {
            bz_PlayerJoinPartEventData_V1 *join = (bz_PlayerJoinPartEventData_V1 *)eventData;

            if (!join->record)
            {
                return;
            }

            if (join->record->team != eObservers && isGameInProgress)
            {
                bz_sendTextMessage(BZ_SERVER, join->playerID, "There is a currently a match, in progress you have become an observer.");
                bztk_changeTeam(join->playerID, eObservers);
            }
        }

        case bz_eTickEvent:
        {
            if (isCountdownInProgress)
            {
                time_t currentTime;
                time(&currentTime);

                if (difftime(currentTime, lastCountdownCheck) >= 1)
                {
                    if (countdownProgress < 1)
                    {
                        isCountdownInProgress = false;
                        isGameInProgress = true;

                        bz_setBZDBDouble("_gravity", bzdb_gravity);
                        bz_setBZDBDouble("_jumpVelocity", bzdb_jumpVelocity);
                        bz_setBZDBDouble("_reloadTime", bzdb_reloadTime);
                        bz_setBZDBDouble("_tankAngVel", bzdb_tankAngVel);
                        bz_setBZDBDouble("_tankSpeed", bzdb_tankSpeed);

                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The game has started. Good luck!");
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The player at the bottom of the scoreboard will be removed every %d seconds.", kickTime);
                    }
                    else
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%i", countdownProgress);
                        time(&lastCountdownCheck);
                        countdownProgress--;
                    }
                }
            }

            if (isGameInProgress)
            {
                if (bz_getPlayerCount() > 1)
                {
                    time_t currentTime;
                    time(&currentTime);
                    int timeRemaining = difftime(currentTime, lastKickTime);

                    if (timeRemaining >= 60)
                    {
                        if (!firstRun)
                        {
                            if (getPlayerWithLowestScore() < 0)
                            {
                                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Multiple players with lowest score ... nobody gets kicked" );
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Next kick in %d seconds ... ", kickTime );
                            }
                            else
                            {
                                bz_BasePlayerRecord *lastPlace = bz_getPlayerByIndex(getPlayerWithLowestScore());
                                bztk_changeTeam(lastPlace->playerID, eObservers);
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Kicked player with lowest score - \"%s\" (score: %d) - next kick in %d seconds", lastPlace->callsign.c_str(), (lastPlace->wins - lastPlace->losses), kickTime);
                                bz_freePlayerRecord(lastPlace);
                            }
                        }
                        else
                        {
                            firstRun = false;
                        }

                        time(&lastKickTime);
                    }
                    else if (timeRemaining != 0 && timeRemaining % 30 == 0 && difftime(currentTime, lastCountdownCheck) > 1)
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%d seconds until the next player elimination", timeRemaining);
                        time(&lastCountdownCheck);
                    }
                    else if (timeRemaining >= 55 && difftime(currentTime, lastCountdownCheck) >= 1)
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%d...", 60 - timeRemaining);
                        time(&lastCountdownCheck);
                    }
                }
                else if (bz_getPlayerCount() == 1)
                {
                    bz_BasePlayerRecord *lastTankStanding = bz_getPlayerByIndex(getLastTankStanding());
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Last Tank Standing is over! The winner is \"%s\" with a score of %d", lastTankStanding->callsign.c_str(), (lastTankStanding->wins - lastTankStanding->losses));
                    bz_freePlayerRecord(lastTankStanding);

                    isCountdownInProgress = false;
                    isGameInProgress = false;
                }
                else
                {
                    // Meh?
                    isCountdownInProgress = false;
                    isGameInProgress = false;
                }
            }
        }

        default: break;
    }
}

bool lastTankStanding::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "start" && bz_hasPerm(playerID, "vote"))
    {
        if (!isGameInProgress && !isCountdownInProgress && bztk_getPlayerCount() > 2)
        {
            isCountdownInProgress = true;
            firstRun = true;
            time(&lastCountdownCheck);

            countdownProgress = countdownLength;

            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s started a new game of Last Tank Standing. New players are now unable to join.", bz_getPlayerByIndex(playerID)->callsign.c_str());

            bztk_foreachPlayer(resetPlayerScore);
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "All scores have been reset.");

            bzdb_gravity = bz_getBZDBDouble("_gravity");
            bzdb_jumpVelocity = bz_getBZDBDouble("_jumpVelocity");
            bzdb_reloadTime = bz_getBZDBDouble("_reloadTime");
            bzdb_tankAngVel = bz_getBZDBDouble("_tankAngVel");
            bzdb_tankSpeed = bz_getBZDBDouble("_tankSpeed");

            bz_setBZDBDouble("_gravity", -1000.000000);
            bz_setBZDBDouble("_jumpVelocity", 0.000000);
            bz_setBZDBDouble("_reloadTime", 0.1);
            bz_setBZDBDouble("_tankAngVel", 0.000001);
            bz_setBZDBDouble("_tankSpeed", 0.000001);
        }
        else if (isCountdownInProgress)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a countdown in progress.");
        }
        else if (isGameInProgress)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game of Last Tank Standing in progress.");
        }
        else if (bztk_getPlayerCount() <= 2)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "More than 2 players are required to play a game of Last Tank Standing.");
        }

        return true;
    }
    else if (command == "end" && bz_hasPerm(playerID, "gameover"))
    {
        if (isGameInProgress || isCountdownInProgress)
        {
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has ended the current game of Last Tank Standing.", bz_getPlayerByIndex(playerID)->callsign.c_str());

            isCountdownInProgress = false;
            isGameInProgress = false;
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active game of Last Tank Standing.");
        }

        return true;
    }

    if (command == "start" || command == "end")
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to use the /%s command.", command.c_str());
        return true;
    }
}