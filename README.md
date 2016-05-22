# Last Tank Standing

A BZFlag game mode where the last tank standing wins the match. A match of Last Tank Standing consists of multiple rounds, where a round is defined as the time period until someone is eliminated for being in last place. When a match is declared, all tanks are rendered immobile and cannot shoot until the match has started. During the countdown, last minute players are allowed to join or rejoin if they are not satisfied with their starting position. Once a game has started however, all new joiners will automatically be moved to the observer team and must wait for the next game to start. Everyone's scores are reset to 0 when the match is declared and when the countdown is over in case a stray bullet hits someone during the countdown so there is no need to rejoin to start from 0. After every 60 seconds (this value can be configured via a BZDB variable), the player with the lowest score will be eliminated (i.e. moved to the observer team) and in the case of a tie for last place, no player will be eliminated.

## Compiling

If you need assistance compiling this plug-in, take a look at my [compiling instructions](https://github.com/allejo/docs/wiki/BZFlag-Plugin-Distribution) for all of my plug-ins.

## Usage

To use this plugin after it has been compiled, simply load the plugin via the configuration file.

```
-loadplugin /path/to/lastTankStanding.so
```

If you would like to override the default permissions required for /start and /end for a tournament environment, use the configuration file to specify the permissions required to use the commands and pass the configuration as a parameter when loading the plugin.

```
-loadplugin /path/to/lastTankStanding.so,/path/to/lastTankStanding.cfg
```

### Custom BZDB Variables

_ltsKickTime
_ltsCountdown
_ltsIdleKickTime
_ltsResetScoreOnElimination

_ltsKickTime

- Default: *60*
- Description: The number of seconds between each elimination round (i.e. the amount of seconds to wait before eliminating the player with the lowest score).

_ltsCountdown

- Default: *15*
- Description: The number of seconds for the countdown before a game of Last Tank Standing starts.

_ltsIdleKickTime

- Default: *30*
- Description: The numer of seconds to eliminate a player for idling or pausing during a match.

_ltsResetScoreOnElimination

- Default *false*
- Description: Whether or not all of the players' scores should be reset to 0 after each elimination round. When set to true, after a player is eliminated all of the players will start the new round with a score of 0.

### Using Custom BZDB Variables

Because this plugin utilizes custom BZDB variables, using `-set _ltsKickTime 90` in a configuration file or in an options block will cause an error; instead, `-setforced` must be used to set the value of the custom variable: `-setforced _ltsKickTime 90`. These variables can be set and changed normally in-game with the `/set` command.

### Custom Slash Commands

```
/start
/end
```

/start

- Permission Requirement: vote
- Description: Start a new match of Last Tank Standing.

/end

- Permission Requirement: gameover
- Description: End the current game of Last Tank Standing.

**Notice:** *These default permission requirements can be modified by using the optional configuration file to set your own permissions.*

### Configuration File Options

The configuration file is entirely optional and is only used to specify custom permissions for the `/start` and `/end` slash commands; if the specified permission is not a default permission with BZFS, it will be automatically created meaning you can create your own "lts" permission. If a configuration file is not specified, the plugin will use the default permissions specificed above in the "Custom Slash Commands" section.

**Notice:** *When using the configuration file, do __not__ use single or double quotes when specifying the permissions.*

GAME_END_PERM

- Default: gameover
- Description: The permission required for the /end command.

GAME_START_PERM

- Default: ban
- Description: The permission required for the /start command.

License
-------

[MIT](https://github.com/allejo/lastTankStanding/blob/master/LICENSE.md)
