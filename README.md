Language: EN | [RU](/README.ru.md)

![image](./logo.png)

# TsarAPI

Plugin for AmxModX to interact and connect your Counter-Strike 1.6 game server with the service [Tsarvar](https://tsarvar.com/): 
* Displaying chat and events (kills, map changes) in real-time;
* Displaying team scores and lineup;
* Displaying player statistics based on [CSstatsX SQL](https://dev-cs.ru/resources/179/), [CsStats MySQL](https://fungun.net/shop/?p=show&id=3) and their modifications;
* Displaying the ban list based on ban systems compatible with AmxBans ([Fresh Bans](https://dev-cs.ru/resources/196/), [Lite Bans](https://dev-cs.ru/resources/352/)).

## Requirements
* [AmxModX 1.8.3-dev-git4537](https://github.com/alliedmodders/amxmodx);
* Module [AmxxEasyHttp v1.2.0+](https://github.com/Next21Team/AmxxEasyHttp);

## Installation
* Install [AmxxEasyHttp](https://github.com/Next21Team/AmxxEasyHttp);
* Copy the configuration file `/addons/amxmodx/configs/tsarapi.cfg` to the corresponding folder;
* Configure `tsarapi_token` cvar;
* Compile the plugin and copy it to `/addons/amxmodx/plugins`, then enable it in plugins.ini.

## Plugin Configuration
```c
// Token for API usage
tsarapi_token ""

// Send CsStatsX SQL (0.7.2+) / CsStats MySQL player statistics (once a day)
tsarapi_send_csstats "1"

// Send ban list (once a day). Supported systems are AmxBans, Fresh Bans, and Lite Bans
tsarapi_send_amxbans "1"

// Send events for new chat messages
tsarapi_send_chat "1"

// Send events for new kill messages (killfeed)
tsarapi_send_killfeed "1"

// Send player state events: their scores, deaths, team
tsarapi_send_scoreboard "1"

// Send map change events
tsarapi_send_changelevel "1"
```