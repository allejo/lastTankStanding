#include "bzfsAPI.h"
#include "plugin_utils.h"
#include "bztoolkit.h"

void resetPlayerScore(int playerID)
{
    bz_setPlayerWins(playerID, 0);
    bz_setPlayerLosses(playerID, 0);
}

class lastTankStanding : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "Last Tank Standing";}
    virtual void Init(const char* config);
    virtual void Cleanup(void);
    virtual void Event(bz_EventData *eventData);

    virtual bool SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params);

    bool isCountdownInProgress, isGameInProgress;
    int countdownLength, kickTime;
};

BZ_PLUGIN(lastTankStanding)

void lastTankStanding::Init(const char* commandLine)
{
    bz_debugMessage(4, "lastTankStanding plugin loaded");

    isCountdownInProgress = false;
    isGameInProgress = false;
    countdownLength = 15;
    kickTime = 60;

    Register(bz_eBZDBChange);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);

    bz_setBZDBInt("_ltsKickTime", kickTime, 0, false);
    bz_setBZDBInt("_ltsCountDown", countdownLength, 0, false);

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
            else if (bzdbChange->key == "_ltsCountDown")
            {
                if (atoi(bzdbChange->value.c_str()) > 0)
                {
                    countdownLength = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    countdownLength = 60;
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

        }

        default: break;
    }
}

bool lastTankStanding::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params)
{
    if (command == "start" && bz_hasPerm(playerID, "vote"))
    {
        if (!isGameInProgress && !isCountdownInProgress)
        {
            isCountdownInProgress = true;

            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s started a new game of Last Tank Standing. New players are now unable to join.", bz_getPlayerByIndex(playerID)->callsign.c_str());

            bztk_foreachPlayer(resetPlayerScore);
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "All scores have been reset.");

            bz_setBZDBDouble("_gravity", -1000.000000);
            bz_setBZDBDouble("_jumpVelocity", 0.000000);
            bz_setBZDBDouble("_reloadTime", 0.1);
            bz_setBZDBDouble("_tankSpeed", 0.000001);
            bz_setBZDBDouble("_tankAngVel", 0.000001);
        }
    }
    else if (command == "end" && bz_hasPerm(playerID, "gameover"))
    {

    }
}