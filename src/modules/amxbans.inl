#include <amxmodx>
#include <sqlx>
#include <easy_http>
#include <tsarapi_util>
#include <ezjson_gc>

static bool:isModuleEnabled 
static Handle:g_sqlHandle

static const LIMIT = 300;
static const MAX_PAGES = 10;
static const PARTS_SENDING_INTERVAL = 10;
static const ERROR_RETRY_INTERVAL = 5 * 60;

new static const CVAR_NAME[] = "tsarapi_send_amxbans";

public module_amxbans_cfg() {
	if(isModuleEnabled && !is_supported_amxbans_plugin_installed()) {
		set_cvar_num(CVAR_NAME, 0);
		log_amx("AmxBans/FreshBans/LiteBans are not installed on the server. AmxBans module is disabled.")
	}

	scheduler_task_define(
		"amxbans_fetch",
		"{^"current_page^":0}",
		"@module_amxbans_execute_task",
		"@module_amxbans_get_executing_time"
	)
}

public module_amxbans_init() {
	bind_pcvar_num(register_cvar(CVAR_NAME, "", FCVAR_PROTECTED), isModuleEnabled);

	set_task(0.1, "@module_amxbans_make_tuple", generate_task_id());
}

@module_amxbans_make_tuple() {
	g_sqlHandle = sql_make_amxbans_tuple();
}

@module_amxbans_get_executing_time() {
	return get_next_systime_in(_fmt("%d:00:00", random_num(0, 11)));
}

@module_amxbans_execute_task(taskId) {
	if(!g_sqlHandle) {
		scheduler_task_set_executing_at(taskId, get_systime() + 1);
		return;
	}

	if(!isModuleEnabled) {
		module_amxbans_task_stop_and_schedule_next(taskId);
		scheduler_task_sql_commit_changes(taskId);
		return;
	}

	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");

	module_amxbans_task_execute_step(taskId, page, LIMIT);
}

module_amxbans_task_execute_step(taskId, page, limit) {
	new data[1]; data[0] = taskId;

	sql_make_amxbans_query(
		_fmt("SELECT bid, player_id, player_nick, admin_id, admin_nick, ban_created, ban_length, ban_reason, ban_kicks, expired \ 
			FROM `%s` \
			WHERE server_ip = '%s' \
			LIMIT %d OFFSET %d \
		", sql_get_amxbans_table(), get_amxbans_localhost(), limit, page * limit),
		"@module_amxbans_sql_get_amxbans_page",
		data, sizeof(data)
	);
}

@module_amxbans_sql_get_amxbans_page(failstate, Handle:query, error[], errnum, data[], size) {
	ASSERT(failstate == TQUERY_SUCCESS, error);

	new taskId = data[0];

	enum { 
		field_bid, field_player_steamid, field_player_name, 
		field_admin_steamid, field_admin_name,
		field_ban_created, field_ban_length, field_ban_reason,
		field_ban_kicks, field_is_expired
	};

	new playerSteamId[MAX_AUTHID_LENGTH], playerName[MAX_NAME_LENGTH], 
		adminSteamId[MAX_AUTHID_LENGTH], adminName[MAX_NAME_LENGTH], 
		banReason[64], banCreatedAt[32];

	new EzJSON_GC:gc = ezjson_gc_init();
	new EzJSON:root = request_api_object_init(gc);
	new EzJSON:items = ezjson_object_get_value(root, "items");

	for(; SQL_MoreResults(query); SQL_NextRow(query)) {
		SQL_ReadResult(query, field_ban_reason, banReason, charsmax(banReason));
		SQL_ReadResult(query, field_player_steamid, playerSteamId, charsmax(playerSteamId));
		SQL_ReadResult(query, field_player_name, playerName, charsmax(playerName));
		SQL_ReadResult(query, field_admin_steamid, adminSteamId, charsmax(adminSteamId));
		SQL_ReadResult(query, field_admin_name, adminName, charsmax(adminName));
		format_timestamp(banCreatedAt, charsmax(banCreatedAt), SQL_ReadResult(query, field_ban_created));

		new EzJSON:item = ezjson_init_object(); gc += item;
		new EzJSON:data = ezjson_init_object(); gc += data;
		
		ezjson_object_set_number(data, "id", SQL_ReadResult(query, field_bid));
		ezjson_object_set_string(data, "player_steamid", playerSteamId);
		ezjson_object_set_string(data, "player_name", playerName);
		ezjson_object_set_string(data, "admin_steamid", adminSteamId);
		ezjson_object_set_string(data, "admin_name", adminName);
		ezjson_object_set_string(data, "ban_reason", banReason);
		ezjson_object_set_number(data, "ban_length", SQL_ReadResult(query, field_ban_length));
		ezjson_object_set_number(data, "ban_kicks", SQL_ReadResult(query, field_ban_kicks));
		ezjson_object_set_string(data, "ban_created_at", banCreatedAt);
		ezjson_object_set_bool(data, "is_expired", bool:SQL_ReadResult(query, field_is_expired));

		ezjson_object_set_string(item, "type", "ban_entry");
		ezjson_object_set_value(item, "data", data);

		ezjson_array_append_value(items, item);
	}

	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");

	ezjson_object_set_number(root, "offset", page * LIMIT);

	new userData[2]; 
	userData[0] = taskId;
	userData[1] = SQL_NumResults(query) >= LIMIT;
	request_api_object_post(root, "@module_amxbans_on_post_request_complete", userData, sizeof(userData));

	ezjson_gc_destroy(gc);
}

@module_amxbans_on_post_request_complete(EzHttpRequest:request_id) {
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
	
	if(isShouldContinueTask) module_amxbans_task_continue(taskId);
	else module_amxbans_task_stop_and_schedule_next(taskId);

	scheduler_task_sql_commit_changes(taskId);
}

module_amxbans_task_continue(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId);
	new page = ezjson_object_get_number(st, "current_page");

	if(page < MAX_PAGES) {
		ezjson_object_set_number(st, "current_page", page + 1);
		scheduler_task_set_state(taskId, st);
		scheduler_task_set_executing_at(taskId, get_systime() + PARTS_SENDING_INTERVAL);
	}
}

module_amxbans_task_stop_and_schedule_next(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId);

	ezjson_object_set_number(st, "current_page", 0);
	scheduler_task_set_executing_at(taskId, scheduler_task_get_next_execution_time(taskId));
}

Handle:sql_make_amxbans_tuple() {
	static host[64], user[64], pass[64], db[64];

	new configDir[PLATFORM_MAX_PATH]; 
	get_configsdir(configDir, charsmax(configDir));

	if(cvar_exists("fb_sql_host")) {
		server_cmd("exec %s/fb/main.cfg", configDir);
		server_exec();

		if(get_cvar_num("fb_use_sql") != 1) {
			log_amx("fb_use_sql values other than 1 are not supported");
			return Empty_Handle;
		}
		
		get_cvar_string("fb_sql_host", host, charsmax(host));
		get_cvar_string("fb_sql_user", user, charsmax(user));
		get_cvar_string("fb_sql_pass", pass, charsmax(pass));
		get_cvar_string("fb_sql_db", db, charsmax(db));

		set_cvar_string("fb_sql_pass", "***hidden***");
	}
	else if(cvar_exists("lb_sql_host")) {
		get_cvar_string("lb_sql_host", host, charsmax(host));
		get_cvar_string("lb_sql_user", user, charsmax(user));
		get_cvar_string("lb_sql_pass", pass, charsmax(pass));
		get_cvar_string("lb_sql_db", db, charsmax(db));
	}
	else {
		server_cmd("exec %s/sql.cfg", configDir);
		server_exec();
		
		return SQL_MakeStdTuple();
	}

	return SQL_MakeDbTuple(host, user, pass, db);
}

sql_get_amxbans_table() {
	static tableName[64];
	new prefix[12] = "amx";

	if(cvar_exists("fb_sql_table")) {
		get_cvar_string("fb_sql_table", tableName, charsmax(tableName));
		return tableName;
	}
	else if(cvar_exists("lb_sql_pref"))
		get_cvar_string("lb_sql_pref", prefix, charsmax(prefix));
	else if(cvar_exists("amx_sql_prefix"))
		get_cvar_string("amx_sql_prefix", prefix, charsmax(prefix));

	formatex(tableName, charsmax(tableName), "%s_bans", prefix);
	return tableName;
}

sql_make_amxbans_query(const query[], const handler[], const data[] = "", len = 0) {
	ASSERT(g_sqlHandle, "Trying to send SQL query without connection tuple");

	SQL_ThreadQuery(g_sqlHandle, handler, query, data, len);
}

get_amxbans_localhost() {
	static address[MAX_IP_WITH_PORT_LENGTH];

	if(cvar_exists("fb_server_ip")) {
		get_cvar_string("fb_server_ip", address, charsmax(address));
		new port[6]; get_cvar_string("fb_server_port", port, charsmax(port));
		formatex(address, charsmax(address), "%s:%s", address, port);
	}
	else if(cvar_exists("lb_server_ip"))
		get_cvar_string("lb_server_ip", address, charsmax(address));
	else if(cvar_exists("amxbans_server_address"))
		get_cvar_string("amxbans_server_address", address, charsmax(address));

	if(strlen(address) < 9)
		get_user_ip(0, address, charsmax(address));

	return address;
}

bool:is_supported_amxbans_plugin_installed() {
	return cvar_exists("amxbans_server_address") || cvar_exists("fb_server_ip") || cvar_exists("lb_server_ip");
}