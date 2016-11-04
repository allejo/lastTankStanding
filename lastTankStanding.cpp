/*
    Copyright (C) 2013-2016 Vladimir "allejo" Jimenez

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <algorithm>
#include <memory>
#include <time.h>
#include <vector>

#include "bzfsAPI.h"
#include "bztoolkit/bzToolkitAPI.h"
#include "plugin_config.h"

// Define plugin name
std::string PLUGIN_NAME = "Last Tank Standing";

// Define plugin version numbering
int MAJOR = 1;
int MINOR = 1;
int REV = 0;
int BUILD = 76;

// A function that will reset the score for a specific player
void resetPlayerScore(int playerID)
{
    bz_setPlayerWins(playerID, 0);
    bz_setPlayerLosses(playerID, 0);
}

// Loop through all the players and see who's not an observer
int getLastTankStanding()
{
    int lastTankStanding = -1;

    // If there is more than one player playing, then that means we don't have the last tank standing
    if (bztk_getPlayerCount() > 1)
    {
        return lastTankStanding;
    }

    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Loop through all of the players in the list
    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        if (bz_getPlayerByIndex(playerList->get(i))->team != eObservers)
        {
            lastTankStanding = playerList->get(i);
        }
    }

    return lastTankStanding;
}

int getPlayerWithLowestScore()
{
    int playerWithLowestScore = -1;
    int lowestPlayerScore = 99999;
    bool foundDuplicate = false;

    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    for (unsigned int i = 0; i < playerList->size(); i++) // Loop through all the players
    {
        if (bz_getPlayerTeam(playerList->get(i)) == eObservers) // Ignore them if they're an observer
        {
            continue;
        }

        // Get the player score total (so subtract deaths from kills)
        int playerScore = bz_getPlayerWins(playerList->get(i)) - bz_getPlayerLosses(playerList->get(i));

        // If we a player's score is lower than the lowest score recorded
        if (playerScore < lowestPlayerScore)
        {
            lowestPlayerScore = playerScore;
            playerWithLowestScore = playerList->get(i);
            foundDuplicate = false;
        }
        else if (playerScore == lowestPlayerScore) // Someone has the same low score
        {
            foundDuplicate = true;
        }
    }

    // If two people have the same lowest score, then return player ID -1 (a slot that'll never exist)
    if (foundDuplicate)
    {
        playerWithLowestScore = -1;
    }

    return playerWithLowestScore;
}

// Convert a string representation of a boolean to a boolean
static bool toBool (std::string str)
{
    return !str.empty() && (strcasecmp(str.c_str (), "true") == 0 || atoi(str.c_str ()) != 0);
}

class lastTankStanding : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
    enum EliminationReason
    {
        eLowScore = 0, // A player is eliminated for having the lowest score at the end of the round
        eIdleTime = 1, // A player is eliminated for idling too long
        eForfeit = 2,  // A player is eliminated for leaving the match
        eKick = 3,     // A player is eliminated by an admin for being kicked
        eWinner = 4    // A player is "eliminated" for being the winner of the match
    };

    virtual const char* Name () { return bztk_pluginName(); }
    virtual void Init (const char* config);
    virtual void Cleanup (void);
    virtual void Event (bz_EventData *eventData);
    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    virtual void loadConfiguration (const char* configFile);
    virtual void eliminatePlayer (unsigned int playerID, EliminationReason reason);
    virtual void disableMovement (void);
    virtual void enableMovement (void);
    virtual void checkIdleTime (unsigned int playerID);

    virtual void startRecording (void);
    virtual void endRecording (void);
    virtual void endGame (void);

    double
        bzdb_gravity,            // The default values of certain BZDB variables that we will change to disable movement
        bzdb_jumpVelocity,       //     so we need to store the original values for when we reenable movement.
        bzdb_reloadTime,         //
        bzdb_tankSpeed,          //
        bzdb_tankAngVel;         //

    bool
        resetScoreOnElimination, // Whether or not to reset all of the players' scores after each elimination
        isCountdownInProgress,   // Whether or not the countdown to start the game is in progress
        isGameInProgress,        // Whether or not a current match is in progress
        matchRecording,          // Whether or not a recording is in progress
        recordMatch,             // Whether or not to record the LTS match
        firstRun;                // Whether or not this is the first loop in a game to prevent announcing the amount of
                                 //     seconds remaining until the kick at the start of the game

    int
        countdownProgress,       // The number of seconds still left in the countdown
        countdownLength,         // The duration the countdown for a new game should be
        idleKickTime,            // The number of seconds a player is allowed to idle before getting eliminated automatically
        roundNumber,             // The current round number of elimination
        kickTime;                // The duration of each round for player elimination

    std::string
        gameoverPermission,      // The server permission required to end a game
        startPermission,         // The server permission required to start a game
        replayFileName;          // The file name used for the recording

    time_t
        lastCountdownCheck,      // The time stamp used to keep each number of the countdown exactly one second apart
        lastEliminationTime;     // The time stamp of the previous elimination of a player

    struct RoundElimination
    {
        EliminationReason
            reason;

        std::string
            callsign;

        int
            rounds,
            score;
    };

    std::vector<RoundElimination> eliminations;
};

BZ_PLUGIN(lastTankStanding)

void lastTankStanding::Init(const char* commandLine)
{
    // Load an optional configuration file
    loadConfiguration(commandLine);

    // Set plugin variables
    isCountdownInProgress = false;
    isGameInProgress = false;

    // Register our events with Register()
    Register(bz_eBZDBChange);
    Register(bz_eGetAutoTeamEvent);
    Register(bz_eKickEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPausedEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_eTickEvent);

    // Because the team swapping code is far from ideal, there are some issues with tanks moving as observers and getting
    // kicked, so we disable the speed checks
    bz_setBZDBBool("_speedChecksLogOnly", true);

    // Set some custom BZDB variables with default values
    kickTime        = bztk_registerCustomIntBZDB("_ltsKickTime", 60);
    countdownLength = bztk_registerCustomIntBZDB("_ltsCountdown", 15);
    idleKickTime    = bztk_registerCustomIntBZDB("_ltsIdleKickTime", 30);
    resetScoreOnElimination = bztk_registerCustomBoolBZDB("_ltsResetScoreOnElimination", false);

    // Register custom slash commands
    bz_registerCustomSlashCommand("start", this);
    bz_registerCustomSlashCommand("gameover", this);

    // Sanity checks/warnings for server owners
    if (bz_getGameType() != eFFAGame && bz_getGameType() != eOpenFFAGame)
    {
        bz_debugMessage(0, "WARNING :: Last Tank Standing :: This server is not configured as FFA or OpenFFA; this may lead to unexpected behavior.");
    }

    if (bz_getGameType() == eFFAGame)
    {
        if (bz_getTeamPlayerLimit(eRedTeam)  > 0 || bz_getTeamPlayerLimit(eGreenTeam)  > 0 ||
            bz_getTeamPlayerLimit(eBlueTeam) > 0 || bz_getTeamPlayerLimit(ePurpleTeam) > 0)
        {
            bz_debugMessage(0, "WARNING :: Last Tank Standing :: This server is configured with regular teams, an FFA server should only be configured with Rogue players.");
        }
    }

    if (bz_isTimeManualStart())
    {
        bz_debugMessage(0, "WARNING :: Last Tank Standing :: This server is configured with '-timemanual'; this may lead to unexpected behavior. This plug-in");
        bz_debugMessage(0, "                                 has its own countdown functionality and does not rely on '-timemanual'. Use the _ltsKickTime BZDB");
        bz_debugMessage(0, "                                 variable instead.");
    }
}

void lastTankStanding::Cleanup(void)
{
    Flush();

    // Remove our commands
    bz_removeCustomSlashCommand("start");
    bz_removeCustomSlashCommand("gameover");
}

void lastTankStanding::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eBZDBChange: // A BZDB variable is changed
        {
            bz_BZDBChangeData_V1* bzdbChange = (bz_BZDBChangeData_V1*)eventData;

            // Check if one of our LTS variables were changed
            if (bzdbChange->key == "_ltsKickTime")
            {
                if (atoi(bzdbChange->value.c_str()) >= 45)
                {
                    kickTime = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    kickTime = 60;
                }
            }
            else if (bzdbChange->key == "_ltsIdleKickTime")
            {
                int _idleTime = atoi(bzdbChange->value.c_str());

                if (_idleTime >= 15 && _idleTime <= 45)
                {
                    idleKickTime = _idleTime;
                }
                else
                {
                    idleKickTime = 30;
                }
            }
            else if (bzdbChange->key == "_ltsCountdown")
            {
                if (atoi(bzdbChange->value.c_str()) >= 15)
                {
                    countdownLength = atoi(bzdbChange->value.c_str());
                }
                else
                {
                    countdownLength = 15;
                }
            }
            else if (bzdbChange->key == "_ltsResetScoreOnElimination")
            {
                if (bzdbChange->value == "1" || bzdbChange->value == "true" || bzdbChange->value == "on")
                {
                    resetScoreOnElimination = true;
                }
                else
                {
                    resetScoreOnElimination = false;
                }
            }
        }

        case bz_eGetAutoTeamEvent: // This event is called for each new player is added to a team
        {
            bz_GetAutoTeamEventData_V1* autoTeamData = (bz_GetAutoTeamEventData_V1*)eventData;

            // If a player tries to join during the middle of the game and they aren't joining the Observers team, then
            // let's automatically move them to the observers team
            if (isGameInProgress && autoTeamData->team != eObservers)
            {
                autoTeamData->handled = true;
                autoTeamData->team = eObservers;

                bz_sendTextMessage(BZ_SERVER, autoTeamData->playerID, "There is a currently a match in progress, you have automatically become an observer.");
            }
        }
        break;

        case bz_eKickEvent:
        {
            bz_KickEventData_V1* kickData = (bz_KickEventData_V1*)eventData;

            eliminatePlayer(kickData->kickedID, eKick);
        }
        break;

        case bz_ePlayerPausedEvent: // This event is called each time a playing tank is paused
        {
            bz_PlayerPausedEventData_V1* pauseData = (bz_PlayerPausedEventData_V1*)eventData;

            std::unique_ptr<bz_BasePlayerRecord> pausedPlayer(bz_getPlayerByIndex(pauseData->playerID));

            // If the player exists, is not an observer, is paused, and there's a game in progress, warn them.
            if (pausedPlayer && pausedPlayer->team != eObservers && pauseData->pause && isGameInProgress)
            {
                bz_sendTextMessagef(BZ_SERVER, pauseData->playerID, "Warning: Pausing during a match is unsportsmanlike conduct.");
                bz_sendTextMessagef(BZ_SERVER, pauseData->playerID, "         You will automatically be kicked in %d seconds.", idleKickTime);
            }
        }
        break;

        case bz_ePlayerPartEvent:
        {
            bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;

            eliminatePlayer(partData->playerID, eForfeit);
        }

        case bz_eTickEvent: // Server tick cycle
        {
            // The game countdown is in progress
            if (isCountdownInProgress)
            {
                // Get the current time
                time_t currentTime;
                time(&currentTime);

                // Make sure at least a second has past since our last countdown number
                if (difftime(currentTime, lastCountdownCheck) >= 1)
                {
                    // If we've reached 0, the game has started!
                    if (countdownProgress < 1)
                    {
                        // A BZDB variable that the 'mapchange' plug-in will respect when attempting to '/mapchange' during a LTS match
                        bz_setBZDBBool("_mapchangeDisable", true, 2);

                        isCountdownInProgress = false;
                        isGameInProgress = true;

                        enableMovement();
                        bztk_foreachPlayer(resetPlayerScore);

                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The game has started. Good luck!");
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The player at the bottom of the scoreboard will be removed every %d seconds.", kickTime);

                        time(&lastEliminationTime); // Have the last kick time be the beginning of the game
                    }
                    else // We're still counting down
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%i", countdownProgress);
                        time(&lastCountdownCheck); // Update the time of the last number announced
                        countdownProgress--; // Decrease the amount of seconds we still have to go
                    }
                }
            }

            if (isGameInProgress) // The game is in progress
            {
                if (bztk_getPlayerCount() > 1) // If there are more than one player playing...
                {
                    time_t currentTime;
                    time(&currentTime);
                    int timeRemaining = difftime(currentTime, lastEliminationTime); // Check how much time is remaining

                    // Check whether or not to eliminate a player for idling too long
                    if (!firstRun)
                    {
                        std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

                        for (unsigned int i = 0; i < playerList->size(); i++)
                        {
                            checkIdleTime(playerList->get(i));
                        }
                    }

                    if (timeRemaining >= kickTime) // If we've reached the time to eliminate someone
                    {
                        if (getPlayerWithLowestScore() < 0) // If the player is -1 then that means more than one player has the same low score
                        {
                            bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Multiple players with lowest score ... nobody gets eliminated" );
                            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Next elimination in %d seconds ... ", kickTime );
                        }
                        else
                        {
                            // Make a reference object of the player in last place
                            std::unique_ptr<bz_BasePlayerRecord> lastPlace(bz_getPlayerByIndex(getPlayerWithLowestScore()));

                            // The player doesn't exist for some reason
                            if (!lastPlace)
                            {
                                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Wait. Where'd the player go? Player to be eliminated not found!");
                                return;
                            }

                            // There are only two players left meaning the next one eliminated means the game is
                            // Don't announce next elimination period
                            if (bztk_getPlayerCount() == 2)
                            {
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Player \"%s\" (score: %d) eliminated!", lastPlace->callsign.c_str(), (lastPlace->wins - lastPlace->losses));
                            }
                            else
                            {
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Player \"%s\" (score: %d) eliminated! - next elimination in %d seconds", lastPlace->callsign.c_str(), (lastPlace->wins - lastPlace->losses), kickTime);
                            }

                            eliminatePlayer(lastPlace->playerID, eLowScore);

                            // If we want to reset a player's score after each elimination
                            if (resetScoreOnElimination)
                            {
                               bztk_foreachPlayer(resetPlayerScore);
                            }

                            bztk_changeTeam(lastPlace->playerID, eObservers);
                        }

                        time(&lastEliminationTime); // Update last kick time
                        roundNumber++;
                        firstRun = false;
                    }
                    else if (timeRemaining != 0 && timeRemaining % 15 == 0 && difftime(currentTime, lastCountdownCheck) > 1) // A multiple of 15 seconds is remaining
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%d seconds until the next player elimination.", (kickTime - timeRemaining));
                        time(&lastCountdownCheck);
                    }
                    else if (timeRemaining >= (kickTime - 5) && difftime(currentTime, lastCountdownCheck) >= 1) // Less than 5 seconds remaining
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%d...", kickTime - timeRemaining);
                        time(&lastCountdownCheck);
                    }
                }
                else if (bztk_getPlayerCount() == 1) // Only one player remaining
                {
                    std::unique_ptr<bz_BasePlayerRecord> lastTankStanding(bz_getPlayerByIndex(getLastTankStanding()));

                    if (!lastTankStanding) // Where'd our player go? Meh
                    {
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "What happened to our winner...?");
                    }
                    else // Announce the winner
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Last Tank Standing is over! The winner is \"%s\".", lastTankStanding->callsign.c_str());
                    }

                    // We need to eliminate the winner so we can compleate the scoreboard. Strange concept, I know.
                    eliminatePlayer(lastTankStanding->playerID, eWinner);

                    // Reverse the order of the elimination record so we can get the most recent first
                    std::reverse(eliminations.begin(), eliminations.end());

                    // Display the leaderboard for the LTS match
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Last Tank Standing Scoreboard");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "-----------------------------");

                    int position = 1;

                    for (auto &player : eliminations)
                    {
                        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%02d. %s", position, player.callsign.c_str());

                        switch (player.reason)
                        {
                            case eWinner:
                            case eLowScore:
                            {
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "    Rounds: %d, Elimination Score: %d", player.rounds, player.score);
                            }
                            break;

                            case eForfeit:
                            case eIdleTime:
                            {
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "    Rounds: %d, Elimination Score: %d [Forfeit]", player.rounds, player.score);
                            }
                            break;

                            case eKick:
                            {
                                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "    Rounds: %d, Elimination Score: %d [Disqualified]", player.rounds, player.score);
                            }
                            break;
                        }

                        position++;
                    }

                    endGame();
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The current match was ended automatically with no winner.");

                    endGame();
                }
            }
        }

        default:
            break;
    }
}

bool lastTankStanding::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "start" && bz_hasPerm(playerID, startPermission.c_str())) // Check the permissions, by default any player with voting permissions can start a game
    {
        if (isCountdownInProgress)
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
        else
        {
            // Setup variables and stuff
            time(&lastCountdownCheck);
            isCountdownInProgress = true;
            roundNumber = 1;
            firstRun = true;

            if (params->size() > 0 && atoi(params->get(0).c_str()) >= 15)
            {
                countdownProgress = atoi(params->get(0).c_str());
            }
            else
            {
                countdownProgress = countdownLength;
            }

            startRecording();

            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s started a new game of Last Tank Standing", bz_getPlayerByIndex(playerID)->callsign.c_str());

            // Reset scores and disable movement
            bztk_foreachPlayer(resetPlayerScore);
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "All scores have been reset.");
            disableMovement();
        }

        return true;
    }
    else if (command == "gameover" && bz_hasPerm(playerID, gameoverPermission.c_str())) // Check the permission requirements, by default only admins can end a game
    {
        if (isGameInProgress || isCountdownInProgress) // If there's a game to end, end it
        {
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has ended the current game of Last Tank Standing.", bz_getPlayerCallsign(playerID));

            endGame();
        }
        else // No game to end, silly admin
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active game of Last Tank Standing.");
        }

        return true;
    }

    // No permission to execute these commands. Shame on them!
    if (command == "start" || command == "gameover")
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to use the /%s command.", command.c_str());
        return true;
    }

    return false;
}

void lastTankStanding::loadConfiguration(const char* configFile)
{
    recordMatch        = false;
    startPermission    = "vote";
    gameoverPermission = "endgame";

    if (configFile && configFile[0] != '\0')
    {
        PluginConfig config = PluginConfig(configFile);
        std::string section = "lastTankStanding";

        if (config.errors)
        {
            bz_debugMessagef(0, "Your configuration file has errors and has failed to load. Using default permissions...");
        }
        else
        {
            recordMatch        = toBool(config.item(section, "RECORD_MATCHES"));
            startPermission    = config.item(section, "GAME_START_PERM");
            gameoverPermission = config.item(section, "GAME_END_PERM");
        }
    }

    bz_debugMessagef(2, "DEBUG :: Last Tank Standing :: The /start command requires the '%s' permission.", startPermission.c_str());
    bz_debugMessagef(2, "DEBUG :: Last Tank Standing :: The /end command requires the '%s' permission.", gameoverPermission.c_str());
    bz_debugMessagef(2, "DEBUG :: Last Tank Standing :: LTS Matches %s be recored", (recordMatch) ? "will be" : "will not be");
}

void lastTankStanding::eliminatePlayer(unsigned int playerID, EliminationReason reason)
{
    RoundElimination record;

    record.callsign = bz_getPlayerCallsign(playerID);
    record.score    = bz_getPlayerWins(playerID) - bz_getPlayerLosses(playerID);
    record.rounds   = roundNumber;
    record.reason   = reason;

    eliminations.push_back(record);
}

// Disable tanks from movement and shooting
void lastTankStanding::disableMovement()
{
    // Save variable settings
    bzdb_gravity      = bz_getBZDBDouble("_gravity");
    bzdb_jumpVelocity = bz_getBZDBDouble("_jumpVelocity");
    bzdb_reloadTime   = bz_getBZDBDouble("_reloadTime");
    bzdb_tankAngVel   = bz_getBZDBDouble("_tankAngVel");
    bzdb_tankSpeed    = bz_getBZDBDouble("_tankSpeed");

    // Disable movement and shooting
    bz_setBZDBDouble("_gravity", -1000.000000);
    bz_setBZDBDouble("_jumpVelocity", 0.000000);
    bz_setBZDBDouble("_reloadTime", 0.1);
    bz_setBZDBDouble("_tankAngVel", 0.000001);
    bz_setBZDBDouble("_tankSpeed", 0.000001);
}

// Enable tanks to move and shoot again
void lastTankStanding::enableMovement()
{
    bz_setBZDBDouble("_gravity", bzdb_gravity);
    bz_setBZDBDouble("_jumpVelocity", bzdb_jumpVelocity);
    bz_setBZDBDouble("_reloadTime", bzdb_reloadTime);
    bz_setBZDBDouble("_tankAngVel", bzdb_tankAngVel);
    bz_setBZDBDouble("_tankSpeed", bzdb_tankSpeed);
}


// Switch players if they have idled too long or are paused for too long
void lastTankStanding::checkIdleTime(unsigned int playerID)
{
    // Check the amount of time a player has been idle or paused. We will automatically eliminate players if they idle for too long
    if (bz_getIdleTime(playerID) >= bz_getBZDBDouble("_ltsIdleKickTime"))
    {
        bztk_changeTeam(playerID, eObservers);
        eliminatePlayer(playerID, eIdleTime);

        bz_sendTextMessagef(BZ_SERVER, playerID, "You have been automatically eliminated for idling too long.");
    }
}

void lastTankStanding::startRecording()
{
    if (recordMatch)
    {
        matchRecording = bz_startRecBuf();

        bz_Time time;
        bz_getLocaltime(&time);

        char temp[512];
        sprintf(temp,"lts-%d%02d%02d-%02d%02d%02d.rec",
                time.year,time.month,time.day,
                time.hour,time.minute,time.second);

        replayFileName = temp;
    }
}

void lastTankStanding::endRecording()
{
    if (matchRecording)
    {
        bz_saveRecBuf(replayFileName.c_str());
        bz_stopRecBuf();

        matchRecording = false;
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "LTS replay saved as: %s", replayFileName.c_str());
    }
}

void lastTankStanding::endGame()
{
    bz_setBZDBBool("_mapchangeDisable", false, 2);

    isCountdownInProgress = false;
    isGameInProgress = false;
    roundNumber = 0;

    eliminations.clear();
    enableMovement();
    endRecording();
}
