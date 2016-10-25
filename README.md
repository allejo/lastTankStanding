# Last Tank Standing

[![GitHub release](https://img.shields.io/github/release/allejo/lastTankStanding.svg?maxAge=2592000)](https://github.com/allejo/lastTankStanding/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.4+-blue.svg)
[![License](https://img.shields.io/github/license/allejo/lastTankStanding.svg)](https://github.com/allejo/lastTankStanding/blob/master/LICENSE.md)

A BZFlag FFA game mode, where the player with the lowest score gets eliminated until there's the last tank standing.

A match of Last Tank Standing (LTS) consists of multiple rounds, where a round is defined as the time period where someone gets eliminated (defined by `_ltsKickTime`). When a match is started, all tanks are rendered immobile and cannot shoot until the match has begun. During the countdown, players are allowed to join or rejoin if they are not satisfied with their starting position. Once a game has started however, all players will automatically be moved to the observer team and must wait for the next game to start whether they just joined or were rejoining. Everyone's scores are reset to 0 at the start of the match, when the countdown reaches 0. After every 60 seconds (defined by `_ltsKickTime`), the player with the lowest score will be eliminated (i.e. moved to the observer team) and in the case of a tie for last place, no player will be eliminated.

## Compiling Requirements

- BZFlag 2.4.4+
- [bztoolkit](https://github.com/allejo/bztoolkit)
- C++11

## Usage

### Loading the plug-in

This plug-in accepts the path to a [configuration file](https://github.com/allejo/lastTankStanding/blob/master/lastTankStanding.cfg) when loaded. This configuration file is not mandatory but is needed for some configuration.

```
-loadplugin lastTankStanding,/path/to/lastTankStanding.cfg
```

### Custom BZDB Variables

These custom BZDB variables must be used with `-setforced`, which sets BZDB variable `<name>` to `<value>`, even if the variable does not exist. These variables may changed at any time in-game by using the `/set` command.

```
-setforced <name> <value>
```

| Name | Type | Default | Description |
| ---- | ---- | ------- | ----------- |
| `_ltsKickTime` | int | 60 | The number of seconds between each elimination round |
| `_ltsCountdown` | int | 15 | The default number of seconds for the countdown before a game of LTS starts. This can be changed by specifying the number of seconds with `/start <seconds>` |
| `_ltsIdleKickTime` | int | 30 | The numer of seconds to eliminate a player for idling or pausing during a match |
| `_ltsResetScoreOnElimination` | bool | false | When set to true, all of the remaining players' scores will be reset to 0 |

### Custom Slash Commands

| Command | Permission | Description |
| ------- | ---------- | ----------- |
| `/start <seconds>` | vote | Start a new match of Last Tank Standing |
| `/gameover` | endgame | End the current game of Last Tank Standing |

> **Tip:** The permissions required for these commands may be changed by using the [configuration file](#configuration-file).

### Configuration File

| Field | Type | Description |
| ----- | ---- | ----------- |
| `GAME_START_PERM` | string | The permission required for the `/start` command |
| `GAME_END_PERM` | string | The permission required for the `/gameover` command |
| `RECORD_MATCHES` | bool | Whether or not to record LTS matches and save them as replays; enabling this functionality requires that `-recdir` be set in the BZFS configuration |

> **Warning:** Do **not** use single or double quotes when defining string values in the configuration file.  
> **Tip:** Permissions are case-insensitive.  
> **Tip:** You may use custom permissions such as 'LTS' or 'Bacon' and the plug-in will still behave correctly

## License

[MIT](https://github.com/allejo/lastTankStanding/blob/master/LICENSE.md)