Last Tank Standing
================

A BZFlag game mode where the last tank standing wins the match. A match of Last Tank Standing consists of multiple rounds, where a round is defined as the time period until someone is eliminated for being in last place. When a match is declared, all tanks are rendered immobile and cannot shoot until the match has started. During the countdown, last minute players are allowed to join or rejoin if they are not satisfied with their starting position. Once a game has started however, all new joiners will automatically be moved to the observer team and must wait for the next game to start. Everyone's scores are reset to 0 when the match is declared and when the countdown is over in case a stray bullet hits someone during the countdown so there is no need to rejoin to start from 0. After every 60 seconds (this value can be configured via a BZDB variable), the player with the lowest score will be eliminated (i.e. moved to the observer team) and in the case of a tie for last place, no player will be eliminated.

Table of Contents
-----------------

-   [Author](#author)

-   [Compiling](#compiling)

    -   [Requirements](#requirements)

    -   [How to Compile](#how-to-compile)

-   [Server Details](#server-details)

    -   [How to Use](#how-to-use)
    
    -   [Custom BZDB Variables](#custom-bzdb-variables)
    
    -   [Using Custom BZDB Variables](#using-custom-bzdb-variables)
    
    -   [Custom Slash Commands](#custom-slash-commands)

-   [License](#license)

Author
------

Vladimir "allejo" Jimenez

Compiling
---------

### Requirements

- BZFlag 2.4.0+

- [bzToolkit](https://github.com/allejo/bztoolkit/)

### How To Compile

1.  Check out the BZFlag source code.

    `git clone -b v2_4_x https://github.com/BZFlag-Dev/bzflag-import-3.git bzflag`

2.  Go into the newly checked out source code and then the plugins directory.

    `cd bzflag/plugins`

3.  Create a plugin using the `newplug.sh` script.

    `sh newplug.sh lastTankStanding`

4.  Delete the newly create lastTankStanding directory.

    `rm -rf lastTankStanding`

5.  Run a git clone of this repository from within the plugins directory. This should have created a new lastTankStanding directory within the plugins directory.

    `git clone https://github.com/allejo/lastTankStanding.git`

6.  Now you will need to checkout the required submodules so the plugin has the proper dependencies so it can compile properly.

    `cd lastTankStanding; git submodule update --init`

7.  Instruct the build system to generate a Makefile and then compile and install the plugin.

    `cd ..; ./autogen.sh; ./configure; make; make install;`

Server Details
--------------

### How to Use

To use this plugin after it has been compiled, simply load the plugin via the configuration file.

`-loadplugin /path/to/lastTankStanding.so`

### Custom BZDB Variables

    _ltsKickTime
    _ltsCountdown
    _ltsResetScoreOnElimination

_ltsKickTime

- Default: *60*

- Description: The number of seconds between each elimination round (i.e. the amount of seconds to wait before eliminating the player with the lowest score).

_ltsCountdown

- Default: *15*

- Description: The number of seconds for the countdown before a game of Last Tank Standing starts.

_ltsResetScoreOnElimination

- Default *false*

- Description: Whether or not all of the players' scores should be reset to 0 after each elimination round. When set to true, after a player is eliminated all of the players will start the new round with a score of 0.

### Using Custom BZDB Variables

Because this plugin utilizes custom BZDB variables, using `-set _ltsKickTime 90` in a configuration file or in an options block will cause an error; instead, `-setforced` must be used to set the value of the custom variable: `-setforced _ltsKickTime 90`. These variables can be set and changed normally in-game with the `/set` command.

### Custom Slash Commands

    /start
    /end

/start

- Permission Requirement: Vote

- Description: Start a new match of Last Tank Standing.

/end

- Permission Requirement: Gameover

- Description: End the current game of Last Tank Standing.

License
-------

[GNU General Public License v3](https://github.com/allejo/lastTankStanding/blob/master/LICENSE.markdown)
