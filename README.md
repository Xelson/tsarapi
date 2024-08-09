Language: EN | [RU](/README.ru.md)

![image](./logo.png)

# Trsarapi

Plugin for AmxModX to interact and connect your Counter-Strike 1.6 game server with the service [Tsaravar](https://tsarvar.com/): 
* Displaying chat and events (kills, map changes) in real-time;
* Displaying team scores and compositions;
* Displaying player statistics based on [CSstatsX SQL](https://dev-cs.ru/resources/179/), [CsStats MySQL](https://fungun.net/shop/?p=show&id=3) and their modifications;
* Displaying the ban list based on ban systems compatible with AmxBans ([Fresh Bans](https://dev-cs.ru/resources/196/), [Lite Bans](https://dev-cs.ru/resources/352/))

## Requirements
* [AmxModX 1.9.0+](https://github.com/alliedmodders/amxmodx)
* Module [AmxxEasyHttp v1.2.0+](https://github.com/Next21Team/AmxxEasyHttp)
* MySQL database

## Installation
* Install [AmxxEasyHttp](https://github.com/Next21Team/AmxxEasyHttp)
* Copy the configuration file `/addons/amxmodx/configs/tsarapi.cfg` to the corresponding folder
* Compile the plugin and copy it to `/addons/amxmodx/plugins`, then enable it in plugins.ini

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

// MySQL database credentials for the task scheduler
tsarapi_sql_host "localhost"
tsarapi_sql_user ""
tsarapi_sql_pass ""
tsarapi_sql_db ""
```