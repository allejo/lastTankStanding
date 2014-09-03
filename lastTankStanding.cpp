/*
Last Tank Standing
    Copyright (C) 2013-2014 Vladimir Jimenez

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <memory>
#include <time.h>

#include "bzfsAPI.h"
#include "bztoolkit/bzToolkitAPI.h"
#include "plugin_config.h"

// Define plugin name
const std::string PLUGIN_NAME = "Last Tank Standing";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 3;
const int BUILD = 57;

// Switch players if they have idled too long or are paused for too long
void checkIdleTime(int playerID)
{
    // Check the amount of time a player has been idle or paused
    if (bz_getIdleTime(playerID) >= bz_getBZDBDouble("_ltsIdleKickTime"))
    {
        // We will automatically eliminate players if they idle for too long
        bztk_changeTeam(playerID, eObservers);
        bz_sendTextMessagef(BZ_SERVER, playerID, "You have been automatically eliminated for idling too long.");
    }
}

// A function that will reset the score for a specific player
void resetPlayerScore(int playerID)
{
    bz_setPlayerWins(playerID, 0);
    bz_setPlayerLosses(playerID, 0);
}

// Loop through all the players and see who's not an observer
int getLastTankStanding()
{
    // By default, we will select the winner to be -1
    int lastTankStanding = -1;

    // If there is more than one player playing, then that means we don't have the last tank standing
    if (bztk_getPlayerCount() > 1)
    {
        return lastTankStanding;
    }

    // Get the list of all of the players in a smart pointer
    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Loop through all of the players in the list
    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        // If the player is not an observer, then that means we've found the last player standing
        if (bz_getPlayerByIndex(playerList->get(i))->team != eObservers)
        {
            lastTankStanding = playerList->get(i);
        }
    }

    return lastTankStanding;
}

// Loop through all the players and find the one with the lowest score
int getPlayerWithLowestScore()
{
    // Variables
    int playerWithLowestScore = -1;
    int lowestHighestScore = 99999;
    bool foundDuplicate = false;

    // Get the list of all of the players in a smart pointer
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
        if (playerScore < lowestHighestScore)
        {
            // Set the new lowest score
            lowestHighestScore = playerScore;

            // Save the player ID
            playerWithLowestScore = playerList->get(i);

            // There are no duplicate scores
            foundDuplicate = false;
        }
        else if (playerScore == lowestHighestScore) // Someone has the same low score
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

class lastTankStanding : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name ();
    virtual void Init (const char* config);
    virtual void Cleanup (void);
    virtual void Event (bz_EventData *eventData);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    virtual void loadConfiguration (const char* configFile);
    virtual void disableMovement (void);
    virtual void enableMovement (void);

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
        firstRun;                // Whether or not this is the first loop in a game to prevent announcing the amount of
                                 //     seconds remaining until the kick at the start of the game

    int
        countdownProgress,       // The number of seconds still left in the countdown
        countdownLength,         // The duration the countdown for a new game should be
        idleKickTime,            // The number of seconds a player is allowed to idle before getting eliminated automatically
        kickTime;                // The duration of each round for player elimination

    std::string
        startPermission,         // The server permission required to start a game
        gameoverPermission;      // The server permission required to end a game

    time_t
        lastCountdownCheck,      // The time stamp used to keep each number of the countdown exactly one second apart
        lastEliminationTime;     // The time stamp of the previous elimination of a player
};

BZ_PLUGIN(lastTankStanding)

const char* lastTankStanding::Name (void)
{
    static std::string pluginBuild = "";

    if (!pluginBuild.size())
    {
        std::ostringstream pluginBuildStream;

        pluginBuildStream << PLUGIN_NAME << " " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();
    }

    return pluginBuild.c_str();
}

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
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPausedEvent);
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
    bz_registerCustomSlashCommand("end", this);
}

void lastTankStanding::Cleanup(void)
{
    Flush();

    // Remove our commands
    bz_removeCustomSlashCommand("start");
    bz_removeCustomSlashCommand("end");
}

void lastTankStanding::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eBZDBChange: // A BZDB variable is changed
        {
            bz_BZDBChangeData_V1* bzdbChange = (bz_BZDBChangeData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString)  key       - The variable that was changed
            //    (bz_ApiString)  value     - What the variable was changed too
            //    (double)        eventTime - This value is the local server time of the event.

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
                if (bzdbChange->value == "1" || bzdbChange->value == "true")
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

            // Data
            // ---
            //    (int)           playerID  - ID of the player that is being added to the game.
            //    (bz_ApiString)  callsign  - Callsign of the player that is being added to the game.
            //    (bz_eTeamType)  team      - The team that the player will be added to. Initialized to the team chosen by the
            //                                current server team rules, or the effects of a plug-in that has previously processed
            //                                the event. Plug-ins wishing to override the team should set this value.
            //    (bool)          handled   - The current state representing if other plug-ins have modified the default team.
            //                                Plug-ins that modify the team should set this value to true to inform other plug-ins
            //                                that have not processed yet.
            //    (double)        eventTime - This value is the local server time of the event.

            if (!autoTeamData)
            {
                return;
            }

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

        case bz_ePlayerJoinEvent: // A player has joined
        {
            bz_PlayerJoinPartEventData_V1 *join = (bz_PlayerJoinPartEventData_V1 *)eventData;

            // Data
            // ---
            //    (int)                   playerID  - The player ID that is joining
            //    (bz_BasePlayerRecord*)  record    - The player record for the joining player
            //    (double)                eventTime - Time of event.

            // The player was disconnected before they could fully join so ignore them
            if (!join || !join->record)
            {
                return;
            }

            // If they don't join the observer team while a game is in progress, switch them
            if (join->record->team != eObservers && isGameInProgress)
            {
                bz_sendTextMessage(BZ_SERVER, join->playerID, "There is a currently a match in progress, you have automatically become an observer.");
                bztk_changeTeam(join->playerID, eObservers);
            }
        }

        case bz_ePlayerPausedEvent: // This event is called each time a playing tank is paused
        {
            bz_PlayerPausedEventData_V1* pauseData = (bz_PlayerPausedEventData_V1*)eventData;

            // Data
            // ---
            //    (int)     playerID  - ID of the player who paused.
            //    (bool)    pause     - Whether the player is pausing (true) or unpausing (false)
            //    (double)  eventTime - Time local server time for the event.

            std::shared_ptr<bz_BasePlayerRecord> pausedPlayer(bz_getPlayerByIndex(pauseData->playerID));

            // If the player exists, is not an observer, is paused, and there's a game in progress, warn them.
            if (pausedPlayer && pausedPlayer->team != eObservers && pauseData->pause && isGameInProgress)
            {
                bz_sendTextMessagef(BZ_SERVER, pauseData->playerID, "Warning: Pausing during a match is unsportsmanlike conduct.");
                bz_sendTextMessagef(BZ_SERVER, pauseData->playerID, "         You will automatically be kicked in %d seconds.", idleKickTime);
            }
        }
        break;

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
                        // Set the variables
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
                    time_t currentTime; // Get the current time
                    time(&currentTime);
                    int timeRemaining = difftime(currentTime, lastEliminationTime); // Check how much time is remaining

                    // Check whether or not to eliminate a player for idling too long
                    if (!firstRun)
                    {
                        bztk_foreachPlayer(checkIdleTime);
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
                            std::shared_ptr<bz_BasePlayerRecord> lastPlace(bz_getPlayerByIndex(getPlayerWithLowestScore()));

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

                            if (resetScoreOnElimination) // If we want to reset a player's score after each elimination
                            {
                               bztk_foreachPlayer(resetPlayerScore);
                            }

                            // Swap them and clean up
                            bztk_changeTeam(lastPlace->playerID, eObservers);
                        }

                        time(&lastEliminationTime); // Update last kick time
                        firstRun = false;
                    }
                    else if (timeRemaining != 0 && timeRemaining % 15 == 0 && difftime(currentTime, lastCountdownCheck) > 1) // A multiple of 30 seconds is remaining
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
                    // Make a reference
                    std::shared_ptr<bz_BasePlayerRecord> lastTankStanding(bz_getPlayerByIndex(getLastTankStanding()));

                    // Where'd our player go? Meh
                    if (!lastTankStanding)
                    {
                        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "What happened to our winner...?");
                        return;
                    }

                    // Announce the winner
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Last Tank Standing is over! The winner is \"%s\".", lastTankStanding->callsign.c_str());

                    // Reset the variables
                    isCountdownInProgress = false;
                    isGameInProgress = false;
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The current match was ended automatically with no winner.");

                    // No players left and the game is still in progress
                    isCountdownInProgress = false;
                    isGameInProgress = false;
                    enableMovement();
                }
            }
        }

        default: break;
    }
}

bool lastTankStanding::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "start" && bz_hasPerm(playerID, startPermission.c_str())) // Check the permissions, by default any registered players can start a game
    {
        if (!isGameInProgress && !isCountdownInProgress && bztk_getPlayerCount() > 2) // No game in progress, start one!
        {
            // Setup variables and stuff
            firstRun = true;
            isCountdownInProgress = true;
            time(&lastCountdownCheck);

            if (params->size() > 0 && atoi(params->get(0).c_str()) >= 15)
            {
                countdownProgress = atoi(params->get(0).c_str());
            }
            else
            {
                countdownProgress = countdownLength;
            }

            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s started a new game of Last Tank Standing. New players are now unable to join.", bz_getPlayerByIndex(playerID)->callsign.c_str());

            // Reset scores and disable movement
            bztk_foreachPlayer(resetPlayerScore);
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "All scores have been reset.");
            disableMovement();
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
    else if (command == "end" && bz_hasPerm(playerID, gameoverPermission.c_str())) // Check the permission requirements, by default only admins can end a game
    {
        if (isGameInProgress || isCountdownInProgress) // If there's a game to end, end it
        {
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has ended the current game of Last Tank Standing.", bz_getPlayerByIndex(playerID)->callsign.c_str());

            // Reset variables
            isCountdownInProgress = false;
            isGameInProgress = false;

            // Allow tanks to move just in case
            enableMovement();
        }
        else // No game to end, silly admin
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active game of Last Tank Standing.");
        }

        return true;
    }

    // No permission to execute these commands. Shame on them!
    if (command == "start" || command == "end")
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to use the /%s command.", command.c_str());
        return true;
    }
}

void lastTankStanding::loadConfiguration(const char* configFile)
{
    startPermission     = "vote";
    gameoverPermission  = "endgame";

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
            startPermission    = config.item(section, "GAME_START_PERM");
            gameoverPermission = config.item(section, "GAME_END_PERM");
        }
    }

    bz_debugMessagef(2, "DEBUG :: Last Tank Standing :: The /start command requires the '%s' permission.", startPermission.c_str());
    bz_debugMessagef(2, "DEBUG :: Last Tank Standing :: The /end command requires the '%s' permission.", gameoverPermission.c_str());
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
    // Reset BZDB variables from stored values to enable movement
    bz_setBZDBDouble("_gravity", bzdb_gravity);
    bz_setBZDBDouble("_jumpVelocity", bzdb_jumpVelocity);
    bz_setBZDBDouble("_reloadTime", bzdb_reloadTime);
    bz_setBZDBDouble("_tankAngVel", bzdb_tankAngVel);
    bz_setBZDBDouble("_tankSpeed", bzdb_tankSpeed);
}