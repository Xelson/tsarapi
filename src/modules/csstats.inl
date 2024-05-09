#include <amxmodx>
#include <sqlx>
#include <easy_http>
#include <tsarapi_util>
#include <ezjson_gc>

static bool:isModuleEnabled 
static Handle:g_sqlHandle

static const LIMIT = 200;
static const MAX_PAGES = 10;
static const PARTS_SENDING_INTERVAL = 10;
static const ERROR_RETRY_INTERVAL = 5 * 60;

new static TASKID_MAKE_TUPLE;

new static const CVAR_NAME[] = "tsarapi_send_csstats";

enum CsStatsType {
	CSSTATS_TYPE_NONE,
	CSSTATS_TYPE_CSSTATS,
	CSSTATS_TYPE_CSSTATSX
}

public module_csstats_cfg() {
	if(isModuleEnabled && !is_supported_csstats_plugin_installed()) {
		set_cvar_num(CVAR_NAME, 0);
		log_amx("CsStats MySQL/CSstatsX SQL are not installed on the server. CsStats module is disabled.")
	}

	scheduler_task_define(
		"csstats_fetch",
		"{^"current_page^":0}",
		"@module_csstats_execute_task",
		"@module_csstats_get_executing_time"
	)
}

public module_csstats_init() {
	bind_pcvar_num(register_cvar(CVAR_NAME, "", FCVAR_PROTECTED), isModuleEnabled);
	
	TASKID_MAKE_TUPLE = generate_task_id()
	set_task(0.1, "@module_csstats_make_tuple", TASKID_MAKE_TUPLE);
}

@module_csstats_make_tuple() {
	g_sqlHandle = sql_make_csstats_tuple();
}

@module_csstats_get_executing_time() {
	return get_next_systime_in(_fmt("%d:00:00", random_num(12, 23)));
}

@module_csstats_execute_task(taskId) {
	if(!g_sqlHandle) {
		scheduler_task_set_executing_at(taskId, get_systime() + 1);
		return;
	}
	
	if(!isModuleEnabled) {
		module_csstats_task_stop_and_schedule_next(taskId);
		scheduler_task_sql_commit_changes(taskId);
		return;
	}

	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");

	module_csstats_task_execute_step(taskId, page, LIMIT);
}

module_csstats_task_execute_step(taskId, page, limit) {
	new data[1]; data[0] = taskId;

	switch(get_csstats_plugin_type()) {
		case CSSTATS_TYPE_CSSTATS: {
			sql_make_csstats_query(
				_fmt("SELECT id, authid, nick, skill, frags, deaths, headshots, teamkills, \
						shots, hits, damage, defused, planted, explode, gametime, \
						rounds, wint, winct, lasttime \
					FROM `%s` \
					LIMIT %d OFFSET %d \
				", sql_get_csstats_table(), limit, page * limit),
				"@module_csstats_sql_get_csstats_page",
				data, sizeof(data)
			);
		}
		case CSSTATS_TYPE_CSSTATSX: {
			sql_make_csstats_query(
				_fmt("SELECT id, steamid, name, skill, kills, deaths, hs, tks, shots, hits, dmg, \
						bombdefused, bombplants, bombexplosions, connection_time, assists, \
						roundt, wint, roundct, winct, first_join, last_join \
					FROM `%s` \
					LIMIT %d OFFSET %d \
				", sql_get_csstats_table(), limit, page * limit),
				"@module_csstats_sql_get_csstatsx_page",
				data, sizeof(data)
			);
		}
	}
}

@module_csstats_sql_get_csstats_page(failstate, Handle:query, error[], errnum, data[], size) {
	ASSERT(failstate == TQUERY_SUCCESS, error);

	new taskId = data[0];

	enum { 
		field_id, field_steamid, field_name, field_skill, field_kills, field_deaths,
		field_hs, field_tks, field_shots, field_hits, field_dmg, field_bomb_defused,
		field_bomb_plants, field_bomb_explosions, field_connection_time,
		field_rounds, field_round_t_win, field_round_ct_win, field_last_join
	};

	new playerSteamId[MAX_AUTHID_LENGTH], playerName[MAX_NAME_LENGTH], 
		lastJoinTime[32], Float:skill;

	new EzJSON_GC:gc = ezjson_gc_init();
	new EzJSON:root = request_api_object_init(gc);
	new EzJSON:items = ezjson_object_get_value(root, "items");

	for(; SQL_MoreResults(query); SQL_NextRow(query)) {
		SQL_ReadResult(query, field_steamid, playerSteamId, charsmax(playerSteamId));
		SQL_ReadResult(query, field_name, playerName, charsmax(playerName));
		SQL_ReadResult(query, field_skill, skill);
		SQL_ReadResult(query, field_last_join, lastJoinTime, charsmax(lastJoinTime));

		new EzJSON:item = ezjson_init_object(); gc += item;
		new EzJSON:data = ezjson_init_object(); gc += data;
		
		ezjson_object_set_number(data, "id", SQL_ReadResult(query, field_id));
		ezjson_object_set_string(data, "player_steamid", playerSteamId);
		ezjson_object_set_string(data, "player_name", playerName);
		ezjson_object_set_real(data, "skill", skill);
		ezjson_object_set_number(data, "kills", SQL_ReadResult(query, field_kills));
		ezjson_object_set_number(data, "deaths", SQL_ReadResult(query, field_deaths));
		ezjson_object_set_number(data, "headshots", SQL_ReadResult(query, field_hs));
		ezjson_object_set_number(data, "teamkills", SQL_ReadResult(query, field_tks));
		ezjson_object_set_number(data, "shots", SQL_ReadResult(query, field_shots));
		ezjson_object_set_number(data, "hits", SQL_ReadResult(query, field_hits));
		ezjson_object_set_number(data, "dmg", SQL_ReadResult(query, field_dmg));
		ezjson_object_set_number(data, "bomb_defused", SQL_ReadResult(query, field_bomb_defused));
		ezjson_object_set_number(data, "bomb_plants", SQL_ReadResult(query, field_bomb_plants));
		ezjson_object_set_number(data, "bomb_explosions", SQL_ReadResult(query, field_bomb_explosions));
		ezjson_object_set_number(data, "connection_time", SQL_ReadResult(query, field_connection_time));
		ezjson_object_set_number(data, "rounds", SQL_ReadResult(query, field_rounds));
		ezjson_object_set_number(data, "round_t_win", SQL_ReadResult(query, field_round_t_win));
		ezjson_object_set_number(data, "round_ct_win", SQL_ReadResult(query, field_round_ct_win));
		ezjson_object_set_string(data, "last_join", lastJoinTime);

		ezjson_object_set_string(item, "type", "stats_entry");
		ezjson_object_set_value(item, "data", data);

		ezjson_object_set_number(data, "kills", SQL_ReadResult(query, field_kills));
		ezjson_array_append_value(items, item);
	}

	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");

	ezjson_object_set_number(root, "offset", page * LIMIT);

	new userData[2]; 
	userData[0] = taskId;
	userData[1] = SQL_NumResults(query) >= LIMIT;
	request_api_object_post(root, "@module_csstats_on_post_request_complete", userData, sizeof(userData));

	ezjson_gc_destroy(gc);
}

@module_csstats_sql_get_csstatsx_page(failstate, Handle:query, error[], errnum, data[], size) {
	ASSERT(failstate == TQUERY_SUCCESS, error);

	new taskId = data[0];

	enum { 
		field_id, field_steamid, field_name, field_skill, field_kills, field_deaths,
		field_hs, field_tks, field_shots, field_hits, field_dmg, field_bomb_defused,
		field_bomb_plants, field_bomb_explosions, field_connection_time, field_assists,
		field_round_t, field_round_t_win, field_round_ct, field_round_ct_win, field_first_join, field_last_join
	};

	new playerSteamId[MAX_AUTHID_LENGTH], playerName[MAX_NAME_LENGTH], 
		firstJoinTime[32], lastJoinTime[32], Float:skill;

	new EzJSON_GC:gc = ezjson_gc_init();
	new EzJSON:root = request_api_object_init(gc);
	new EzJSON:items = ezjson_object_get_value(root, "items");

	for(; SQL_MoreResults(query); SQL_NextRow(query)) {
		SQL_ReadResult(query, field_steamid, playerSteamId, charsmax(playerSteamId));
		SQL_ReadResult(query, field_name, playerName, charsmax(playerName));
		SQL_ReadResult(query, field_skill, skill);
		SQL_ReadResult(query, field_first_join, firstJoinTime, charsmax(firstJoinTime));
		SQL_ReadResult(query, field_last_join, lastJoinTime, charsmax(lastJoinTime));

		new EzJSON:item = ezjson_init_object(); gc += item;
		new EzJSON:data = ezjson_init_object(); gc += data;
		
		ezjson_object_set_number(data, "id", SQL_ReadResult(query, field_id));
		ezjson_object_set_string(data, "player_steamid", playerSteamId);
		ezjson_object_set_string(data, "player_name", playerName);
		ezjson_object_set_real(data, "skill", skill);
		ezjson_object_set_number(data, "kills", SQL_ReadResult(query, field_kills));
		ezjson_object_set_number(data, "deaths", SQL_ReadResult(query, field_deaths));
		ezjson_object_set_number(data, "headshots", SQL_ReadResult(query, field_hs));
		ezjson_object_set_number(data, "teamkills", SQL_ReadResult(query, field_tks));
		ezjson_object_set_number(data, "shots", SQL_ReadResult(query, field_shots));
		ezjson_object_set_number(data, "hits", SQL_ReadResult(query, field_hits));
		ezjson_object_set_number(data, "dmg", SQL_ReadResult(query, field_dmg));
		ezjson_object_set_number(data, "bomb_defused", SQL_ReadResult(query, field_bomb_defused));
		ezjson_object_set_number(data, "bomb_plants", SQL_ReadResult(query, field_bomb_plants));
		ezjson_object_set_number(data, "bomb_explosions", SQL_ReadResult(query, field_bomb_explosions));
		ezjson_object_set_number(data, "connection_time", SQL_ReadResult(query, field_connection_time));
		ezjson_object_set_number(data, "assists", SQL_ReadResult(query, field_assists));
		ezjson_object_set_number(data, "round_t", SQL_ReadResult(query, field_round_t));
		ezjson_object_set_number(data, "round_t_win", SQL_ReadResult(query, field_round_t_win));
		ezjson_object_set_number(data, "round_ct", SQL_ReadResult(query, field_round_ct));
		ezjson_object_set_number(data, "round_ct_win", SQL_ReadResult(query, field_round_ct_win));
		ezjson_object_set_string(data, "first_join", firstJoinTime);
		ezjson_object_set_string(data, "last_join", lastJoinTime);

		ezjson_object_set_string(item, "type", "stats_entry");
		ezjson_object_set_value(item, "data", data);

		ezjson_array_append_value(items, item);
	}

	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");

	ezjson_object_set_number(root, "offset", page * LIMIT);

	new userData[2]; 
	userData[0] = taskId;
	userData[1] = SQL_NumResults(query) >= LIMIT;
	request_api_object_post(root, "@module_csstats_on_post_request_complete", userData, sizeof(userData));

	ezjson_gc_destroy(gc);
}

@module_csstats_on_post_request_complete(EzHttpRequest:request_id) {
	new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
	ASSERT(ezhttp_get_error_code(request_id) == EZH_OK, "http response error: %s", error);

	new data[2]; ezhttp_get_user_data(request_id, data);
	new taskId = data[0], bool:isShouldContinueTask = bool:data[1];

	if(ezhttp_get_error_code(request_id) != EZH_OK) {
		new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
		log_amx("Error after post request to the API. Rescheduling the task in %d min", ERROR_RETRY_INTERVAL / 60);

		scheduler_task_set_executing_at(taskId, get_systime() + ERROR_RETRY_INTERVAL);
		scheduler_task_sql_commit_changes(taskId);

		return;
	}

	if(isShouldContinueTask) module_csstats_task_continue(taskId);
	else module_csstats_task_stop_and_schedule_next(taskId);

	scheduler_task_sql_commit_changes(taskId);
}

module_csstats_task_continue(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId);
	new page = ezjson_object_get_number(st, "current_page");

	if(page < MAX_PAGES) {
		ezjson_object_set_number(st, "current_page", page + 1);
		scheduler_task_set_state(taskId, st);
		scheduler_task_set_executing_at(taskId, get_systime() + PARTS_SENDING_INTERVAL);
	}
}

module_csstats_task_stop_and_schedule_next(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId);

	ezjson_object_set_number(st, "current_page", 0);
	scheduler_task_set_executing_at(taskId, scheduler_task_get_next_execution_time(taskId));
}

sql_make_csstats_query(const query[], const handler[], const data[] = "", len = 0) {
	ASSERT(g_sqlHandle, "Trying to send SQL query without connection tuple");

	SQL_ThreadQuery(g_sqlHandle, handler, query, data, len);
}

sql_get_csstats_table() {
	static tableName[64];

	switch(get_csstats_plugin_type()) {
		case CSSTATS_TYPE_CSSTATS: get_cvar_string("csstats_table_players", tableName, charsmax(tableName));
		case CSSTATS_TYPE_CSSTATSX: get_cvar_string("csstats_sql_table", tableName, charsmax(tableName));
	}

	return tableName;
}

Handle:sql_make_csstats_tuple() {
	static host[64], user[64], pass[64], db[64];

	switch(get_csstats_plugin_type()) {
		case CSSTATS_TYPE_CSSTATS: {
			get_cvar_string("csstats_sql_host", host, charsmax(host));
			get_cvar_string("csstats_sql_user", user, charsmax(user));
			get_cvar_string("csstats_sql_pass", pass, charsmax(pass));
			get_cvar_string("csstats_sql_db", db, charsmax(db));
		}
		case CSSTATS_TYPE_CSSTATSX: {
			get_cvar_string("csstats_host", host, charsmax(host));
			get_cvar_string("csstats_user", user, charsmax(user));
			get_cvar_string("csstats_pass", pass, charsmax(pass));
			get_cvar_string("csstats_db", db, charsmax(db));
		}
	}

	return SQL_MakeDbTuple(host, user, pass, db);
}

bool:is_supported_csstats_plugin_installed() {
	return get_csstats_plugin_type() != CSSTATS_TYPE_NONE;
}

CsStatsType:get_csstats_plugin_type() {
	if(cvar_exists("csstats_sql_host")) return CSSTATS_TYPE_CSSTATSX;
	else if(cvar_exists("csstats_host")) return CSSTATS_TYPE_CSSTATS;

	return CSSTATS_TYPE_NONE;
}