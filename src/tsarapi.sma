#include <amxmodx>
#include <amxmisc>
#include <sqlx>
#include <easy_http>
#include <ezjson_gc>

#include <amxx_182_compact.inl>

new const PLUGIN[]  	= "Tsarapi"
new const VERSION[] 	= "0.3.1"
new const AUTHOR[]		= "ekke bea?"

new const CONFIG_FILE_NAME[] 	= "tsarapi.cfg";
new const API_URL[] 			= "http://csplugin.tsarvar.com/api";

new const TIMESTAMP_FORMAT[] 		= "%Y-%m-%d %H:%M:%S";

const Float:SEND_QUEUE_EVENTS_INTERVAL = 3.0;
const MAX_TOKEN_LEN = 64;

new g_token[MAX_TOKEN_LEN], g_pluginId;

public plugin_init() {
	g_pluginId = register_plugin(PLUGIN, VERSION, AUTHOR);
	amxx_182_compact_init(g_pluginId) 

	bind_pcvar_string(register_cvar("tsarapi_token", "", FCVAR_PROTECTED), g_token, charsmax(g_token));

	scheduler_worker_init();
	queue_events_worker_init();
	config_observer_init();

	module_chat_init();
	module_amxbans_init();
	module_csstats_init();
	module_killfeed_init();
	module_scoreboard_init();
	module_changelevel_init();

	sql_init();
}

public plugin_cfg() {
	config_exec();

	module_chat_cfg();
	module_amxbans_cfg();
	module_csstats_cfg();
	module_killfeed_cfg();
	module_scoreboard_cfg();
	module_changelevel_cfg();

	sql_connection_make_tuple();
}

new g_sqlHost[64], g_sqlUser[64], g_sqlPassword[128], g_sqlDatabase[64];
new Handle:g_sqlTuple;

static sql_init() {
	bind_pcvar_string(register_cvar("tsarapi_sql_host", "", FCVAR_PROTECTED), g_sqlHost, charsmax(g_sqlHost));
	bind_pcvar_string(register_cvar("tsarapi_sql_user", "", FCVAR_PROTECTED), g_sqlUser, charsmax(g_sqlUser));
	bind_pcvar_string(register_cvar("tsarapi_sql_pass", "", FCVAR_PROTECTED), g_sqlPassword, charsmax(g_sqlPassword));
	bind_pcvar_string(register_cvar("tsarapi_sql_db", "", FCVAR_PROTECTED), g_sqlDatabase, charsmax(g_sqlDatabase));
}

static sql_connection_make_tuple() {
	if(g_sqlTuple != Empty_Handle) SQL_FreeHandle(g_sqlTuple);
	g_sqlTuple = SQL_MakeDbTuple(g_sqlHost, g_sqlUser, g_sqlPassword, g_sqlDatabase, 1);

	scheduler_worker_sql_tuple_init();
}

sql_make_query(const query[], const handler[], const data[] = "", len = 0) {
	ASSERT(g_sqlTuple, "Trying to send SQL query without connection tuple");

	SQL_ThreadQuery(g_sqlTuple, handler, query, data, len);
}

static config_exec() {
	new cfgDir[PLATFORM_MAX_PATH]; 
	get_configsdir(cfgDir, charsmax(cfgDir));

	new configFilePath[PLATFORM_MAX_PATH];
	formatex(configFilePath, charsmax(configFilePath), "%s/%s", cfgDir, CONFIG_FILE_NAME);

	if(file_exists(configFilePath)) {
		server_cmd("exec ^"%s^"", configFilePath);
		server_exec();
	}
}

enum _:STRUCT_QUEUE_EVENT {
	QUEUE_EVENT_NAME[32],
	EzJSON:QUEUE_EVENT_DATA,
	QUEUE_EVENT_HAPPENED_AT
}
new Array:g_arrQueueEvents;

new TASKID_QUEUE_EVENTS_WORKER;

public queue_events_worker_init() {
	TASKID_QUEUE_EVENTS_WORKER = generate_task_id();
	g_arrQueueEvents = ArrayCreate(STRUCT_QUEUE_EVENT);

	set_task(SEND_QUEUE_EVENTS_INTERVAL, "@task_send_queue_events", TASKID_QUEUE_EVENTS_WORKER, .flags = "b");
}

@task_send_queue_events() {
	if(ArraySize(g_arrQueueEvents) > 0) {
		queue_events_http_post(g_arrQueueEvents);
		ArrayClear(g_arrQueueEvents);
	}
}

queue_event_emit(name[], EzJSON:_MOVE_data, replaceIfHandler[] = "", data[] = "", dataLen = 0) {
	new entry[STRUCT_QUEUE_EVENT], index = -1;

	if(strlen(replaceIfHandler) > 0) {
		new Array:arrEventsOfType = ArrayCreate();
		for(new i; i < ArraySize(g_arrQueueEvents); i++) {
			ArrayGetArray(g_arrQueueEvents, i, entry)
			if(!equal(entry[QUEUE_EVENT_NAME], name)) continue;

			ArrayPushCell(arrEventsOfType, entry[QUEUE_EVENT_DATA]);
		}

		if(ArraySize(arrEventsOfType) > 0) {
			new ret = -1, frwd = CreateOneForward(g_pluginId, replaceIfHandler, FP_CELL, FP_ARRAY);
			ExecuteForward(frwd, ret, arrEventsOfType, PrepareArray(data, dataLen));
			ArrayClear(arrEventsOfType);
			DestroyForward(frwd);

			if(ret >= -1 && ret < ArraySize(g_arrQueueEvents)) index = ret
			else abort(AMX_ERR_BOUNDS, "Returned an invalid index %d for events array", ret);
		}	
		else ArrayClear(arrEventsOfType);
	}
	
	copy(entry[QUEUE_EVENT_NAME], charsmax(entry[QUEUE_EVENT_NAME]), name);
	entry[QUEUE_EVENT_DATA] = _MOVE_data;
	entry[QUEUE_EVENT_HAPPENED_AT] = get_systime();

	if(index >= 0)
		ArraySetArray(g_arrQueueEvents, index, entry);

	return ArrayPushArray(g_arrQueueEvents, entry);
}

EzJSON:request_api_object_init(EzJSON_GC:gc) {
	new EzJSON:root = ezjson_init_object();
	gc += root;

	ezjson_object_set_string(root, "token", g_token);

	new EzJSON:items = ezjson_init_array();
	gc += items;

	ezjson_object_set_value(root, "items", items);

	return root;
}

request_api_object_post(EzJSON:object, const handler[], const data[] = "", len = 0) {
	ezjson_object_set_string(object, "ver", VERSION);

	new EzHttpOptions:ezhttpOpt = ezhttp_create_options();
	ezhttp_option_set_header(ezhttpOpt, "Content-Type", "application/json");
	ezhttp_option_set_timeout(ezhttpOpt, 3000);
	ezhttp_option_set_body_from_json(ezhttpOpt, object);
	if(len) ezhttp_option_set_user_data(ezhttpOpt, data, len);

	ezhttp_post(API_URL, handler, ezhttpOpt);
}

static queue_events_http_post(Array:events) {
	new EzJSON_GC:gc = ezjson_gc_init();
	new EzJSON:root = request_api_object_init(gc);
	new EzJSON:items = ezjson_object_get_value(root, "items");

	for(new i, event[STRUCT_QUEUE_EVENT]; i < ArraySize(events); i++) {
		ArrayGetArray(events, i, event);

		new EzJSON:item = ezjson_init_object();
		gc += item;
		gc += event[QUEUE_EVENT_DATA];

		ezjson_object_set_string(item, "type", event[QUEUE_EVENT_NAME]);
		ezjson_object_set_value(item, "data", event[QUEUE_EVENT_DATA]);
		ezjson_object_set_number(item, "happened_at", event[QUEUE_EVENT_HAPPENED_AT]);
		
		ezjson_array_append_value(items, item);
	}

	request_api_object_post(root, "@on_queue_events_post_complete");

	ezjson_gc_destroy(gc);
}

@on_queue_events_post_complete(EzHttpRequest:request_id) {
	new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
	
	if(ezhttp_get_error_code(request_id) != EZH_OK) {
		log_http_error(error);
		return;
	}
}

format_timestamp(output[], len, time = -1) {
	return format_time(output, len, TIMESTAMP_FORMAT, time);
}

parse_timestamp(const input[], time = -1) {
	return parse_time(input, TIMESTAMP_FORMAT, time);
}

get_localhost() {
	static address[MAX_IP_WITH_PORT_LENGTH];
	get_user_ip(0, address, charsmax(address));
	return address;
}

#include <sheduler.inl>
#include <config_observer.inl>
#include <modules/chat.inl>
#include <modules/killfeed.inl>
#include <modules/scoreboard.inl>
#include <modules/amxbans.inl>
#include <modules/csstats.inl>
#include <modules/changelevel.inl>