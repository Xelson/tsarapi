Language: [EN](/README.md) | RU

![image](./logo.png)

# TsarAPI

Плагин для AmxModX для взаимодействия и связи вашего игрового сервера Counter-Strike 1.6 с сервисом [Tsarvar](https://tsarvar.com/):
* Отображение чата и событий (убийства, смены карты) в реальном времени;
* Отображение счета команд и их состав;
* Отображение статистики игроков на базе [CSstatsX SQL](https://dev-cs.ru/resources/179/), [CsStats MySQL](https://fungun.net/shop/?p=show&id=3) и их модификаций;
* Отображение бан-листа на базе бан-систем, совместимых с AmxBans ([Fresh Bans](https://dev-cs.ru/resources/196/), [Lite Bans](https://dev-cs.ru/resources/352/)).

[Пример интеграции с одним из серверов](https://tsarvar.com/ru/@EpicFunKnife/plugin)

## Требования для работы
* [AmxModX 1.9.0+](https://github.com/alliedmodders/amxmodx);
* Модуль [AmxxEasyHttp v1.2.0+](https://github.com/Next21Team/AmxxEasyHttp);
* MySQL база данных.

## Установка

* Установить [AmxxEasyHttp](https://github.com/Next21Team/AmxxEasyHttp);
* Скопировать файл конфигурации `/addons/amxmodx/configs/tsarapi.cfg` в соответствующую папку;
* Сконфигурировать квары `tsarapi_token` и `tsarapi_sql_*`;
* Скомпилировать плагин и скопировать в `/addons/amxmodx/plugins`, подключить в `plugins.ini`.

## Конфигурация плагина

```c
// Токен для использования API
tsarapi_token ""

// Отправлять статистику игроков CsStatsX SQL (0.7.2+) / CsStats MySQL (раз в день)
tsarapi_send_csstats "1"

// Отправлять бан-лист (раз в день). Поддерживается AmxBans, Fresh Bans и Lite Bans
tsarapi_send_amxbans "1"

// Отправлять события об новых сообщениях в чате
tsarapi_send_chat "1"

// Отправлять события об новых сообщениях об убийстве (килфид)
tsarapi_send_killfeed "1"

// Отправлять события об состоянии игроков: их счет, смерти, команду
tsarapi_send_scoreboard "1"

// Отправлять событие смены карты
tsarapi_send_changelevel "1"

// Данные от MySQL базы данных для планировщика задач
tsarapi_sql_host "localhost"
tsarapi_sql_user ""
tsarapi_sql_pass ""
tsarapi_sql_db ""
```

