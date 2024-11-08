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
new static cvarId

enum CsStatsType {
	CSSTATS_TYPE_NONE,
	CSSTATS_TYPE_CSSTATS,
	CSSTATS_TYPE_CSSTATSX
}

public module_csstats_cfg() {
	if(isModuleEnabled && !is_supported_csstats_plugin_installed()) {
		set_pcvar_num(cvarId, 0);
		log_amx("CsStats MySQL/CSstatsX SQL are not installed on the server. CsStats module is disabled.")
	}

	config_observer_watch_cvar(
		cvarId, 
		"send_csstats", 
		config_option_number
	);

	scheduler_task_define(
		"csstats_fetch",
		"{^"current_page^":0}",
		"@module_csstats_execute_task",
		"@module_csstats_get_executing_time"
	)
}

public module_csstats_init() {
	cvarId = register_cvar("tsarapi_send_csstats", "", FCVAR_PROTECTED)
	bind_pcvar_num(cvarId, isModuleEnabled);
	
	TASKID_MAKE_TUPLE = generate_task_id()
	set_task(0.1, "@module_csstats_make_tuple", TASKID_MAKE_TUPLE);

	mcsx_fields_map_init();
}

@module_csstats_make_tuple() {
	g_sqlHandle = sql_make_csstats_tuple();
}

@module_csstats_get_executing_time() {
	return get_next_systime_in(_fmt("%d:%d:00", random_num(12, 23), random_num(0, 60)));
}

@module_csstats_execute_task(taskId) {
	if(!g_sqlHandle) {
		scheduler_task_set_executing_at(taskId, get_systime() + 1);
		return;
	}
	
	if(!isModuleEnabled) {
		module_csstats_task_stop_and_schedule_next(taskId);
		scheduler_task_commit_changes(taskId);
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
				_fmt("SELECT * FROM `%s` ORDER BY lasttime DESC LIMIT %d OFFSET %d", sql_get_csstats_table(), limit, page * limit),
				"@module_csstats_sql_get_page",
				data, sizeof(data)
			);
		}
		case CSSTATS_TYPE_CSSTATSX: {
			sql_make_csstats_query(
				_fmt("SELECT * FROM `%s` ORDER BY last_join DESC LIMIT %d OFFSET %d", sql_get_csstats_table(), limit, page * limit),
				"@module_csstats_sql_get_page",
				data, sizeof(data)
			);
		}
	}
}

@module_csstats_sql_get_page(failstate, Handle:query, error[], errnum, data[], size) {
	if(failstate != TQUERY_SUCCESS) {
		log_sql_error(error);
		return;
	}

	new taskId = data[0];

	new EzJSON_GC:gc = ezjson_gc_init();
	new EzJSON:root = request_api_object_init(gc);
	new EzJSON:items = ezjson_object_get_value(root, "items");

	for(; SQL_MoreResults(query); SQL_NextRow(query)) {
		new EzJSON:item = ezjson_init_object(); gc += item;
		new EzJSON:data = ezjson_init_object(); gc += data;
		
		ezjson_object_map_fields_from_sql(data, query);

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

enum {
	mcsx_fields_map_number,
	mcsx_fields_map_string,
	mcsx_fields_map_real
}

enum _:MODULE_CSSTATS_FIELDS_MAP {
	MCSX_FIELDS_MAP_FIELD_NAME[32],
	MCSX_FIELDS_MAP_TYPE
}

static Trie:g_fieldsMap

mcsx_fields_map_set(const columnName[], const objFieldName[], type) {
	if(g_fieldsMap == Invalid_Trie) {
		g_fieldsMap = TrieCreate();
	}

	new record[MODULE_CSSTATS_FIELDS_MAP];
	copy(record[MCSX_FIELDS_MAP_FIELD_NAME], charsmax(record[MCSX_FIELDS_MAP_FIELD_NAME]), objFieldName);
	record[MCSX_FIELDS_MAP_TYPE] = type;

	TrieSetArray(g_fieldsMap, columnName, record, sizeof(record));
}

bool:mcsx_fields_map_get(const columnName[], output[MODULE_CSSTATS_FIELDS_MAP]) {
	return TrieGetArray(g_fieldsMap, columnName, output, sizeof(output));
}

mcsx_fields_map_init() {
	mcsx_fields_map_set("id", "id", mcsx_fields_map_number);
	mcsx_fields_map_set("authid", "player_steamid", mcsx_fields_map_string);
	mcsx_fields_map_set("nick", "player_name", mcsx_fields_map_string);
	mcsx_fields_map_set("skill", "skill", mcsx_fields_map_real);
	mcsx_fields_map_set("frags", "kills", mcsx_fields_map_number);
	mcsx_fields_map_set("deaths", "deaths", mcsx_fields_map_number);
	mcsx_fields_map_set("headshots", "headshots", mcsx_fields_map_number);
	mcsx_fields_map_set("teamkills", "teamkills", mcsx_fields_map_number);
	mcsx_fields_map_set("shots", "shots", mcsx_fields_map_number);
	mcsx_fields_map_set("hits", "hits", mcsx_fields_map_number);
	mcsx_fields_map_set("damage", "dmg", mcsx_fields_map_number);
	mcsx_fields_map_set("defused", "bomb_defused", mcsx_fields_map_number);
	mcsx_fields_map_set("planted", "bomb_plants", mcsx_fields_map_number);
	mcsx_fields_map_set("explode", "bomb_explosions", mcsx_fields_map_number);
	mcsx_fields_map_set("gametime", "connection_time", mcsx_fields_map_number);
	mcsx_fields_map_set("rounds", "rounds", mcsx_fields_map_number);
	mcsx_fields_map_set("wint", "round_t_win", mcsx_fields_map_number);
	mcsx_fields_map_set("winct", "round_ct_win", mcsx_fields_map_number);
	mcsx_fields_map_set("lasttime", "last_join", mcsx_fields_map_string);

	// CSSTATS_TYPE_CSSTATSX алиасы
	mcsx_fields_map_set("steamid", "player_steamid", mcsx_fields_map_string);
	mcsx_fields_map_set("name", "player_name", mcsx_fields_map_string);
	mcsx_fields_map_set("kills", "kills", mcsx_fields_map_number);
	mcsx_fields_map_set("hs", "headshots", mcsx_fields_map_number);
	mcsx_fields_map_set("tks", "teamkills", mcsx_fields_map_number);
	mcsx_fields_map_set("dmg", "dmg", mcsx_fields_map_number);
	mcsx_fields_map_set("bombdefused", "bomb_defused", mcsx_fields_map_number);
	mcsx_fields_map_set("bombplants", "bomb_plants", mcsx_fields_map_number);
	mcsx_fields_map_set("bombexplosions", "bomb_explosions", mcsx_fields_map_number);
	mcsx_fields_map_set("connection_time", "connection_time", mcsx_fields_map_number);
	mcsx_fields_map_set("assists", "assists", mcsx_fields_map_number);
	mcsx_fields_map_set("roundt", "round_t", mcsx_fields_map_number);
	mcsx_fields_map_set("roundct", "round_ct", mcsx_fields_map_number);
	mcsx_fields_map_set("first_join", "first_join", mcsx_fields_map_string);
	mcsx_fields_map_set("last_join", "last_join", mcsx_fields_map_string);
}

ezjson_object_map_fields_from_sql(EzJSON:object, Handle:query) {
	for(new i, columnName[32], record[MODULE_CSSTATS_FIELDS_MAP]; i < SQL_NumColumns(query); i++) {
		SQL_FieldNumToName(query, i, columnName, charsmax(columnName));

		if(mcsx_fields_map_get(columnName, record)) {
			switch(record[MCSX_FIELDS_MAP_TYPE]) {
				case mcsx_fields_map_number: {
					ezjson_object_set_number(object, record[MCSX_FIELDS_MAP_FIELD_NAME], SQL_ReadResult(query, i));
				}
				case mcsx_fields_map_real: {
					static Float:value; 
					SQL_ReadResult(query, i, value);

					ezjson_object_set_real(object, record[MCSX_FIELDS_MAP_FIELD_NAME], value)
				}
				case mcsx_fields_map_string: {
					static buffer[256];
					SQL_ReadResult(query, i, buffer, charsmax(buffer));

					ezjson_object_set_string(object, record[MCSX_FIELDS_MAP_FIELD_NAME], buffer)
				}
			}
		}
	}
}

@module_csstats_on_post_request_complete(EzHttpRequest:request_id) {
	new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
	if(ezhttp_get_error_code(request_id) != EZH_OK) {
		log_http_error(error);
		return;
	}

	new data[2]; ezhttp_get_user_data(request_id, data);
	new taskId = data[0], bool:isShouldContinueTask = bool:data[1];

	if(ezhttp_get_error_code(request_id) != EZH_OK) {
		new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
		log_amx("Error after post request to the API. Rescheduling the task in %d min", ERROR_RETRY_INTERVAL / 60);

		scheduler_task_set_executing_at(taskId, get_systime() + ERROR_RETRY_INTERVAL);
		scheduler_task_commit_changes(taskId);

		return;
	}

	if(isShouldContinueTask) module_csstats_task_continue(taskId);
	else module_csstats_task_stop_and_schedule_next(taskId);

	scheduler_task_commit_changes(taskId);
}

module_csstats_task_continue(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId);
	new page = ezjson_object_get_number(st, "current_page");

	if(page < MAX_PAGES) {
		ezjson_object_set_number(st, "current_page", page + 1);
		scheduler_task_set_state(taskId, st);
		scheduler_task_set_executing_at(taskId, get_systime() + PARTS_SENDING_INTERVAL);
	}
	else module_csstats_task_stop_and_schedule_next(taskId);
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
		case CSSTATS_TYPE_CSSTATSX: {
			get_cvar_string("csstats_sql_host", host, charsmax(host));
			get_cvar_string("csstats_sql_user", user, charsmax(user));
			get_cvar_string("csstats_sql_pass", pass, charsmax(pass));
			get_cvar_string("csstats_sql_db", db, charsmax(db));
		}
		case CSSTATS_TYPE_CSSTATS: {
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