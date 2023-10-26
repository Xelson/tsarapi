#include <amxmodx>
#include <amxmisc>
#include <easy_http>

#include <modules/chat.inl>
#include <modules/killfeed.inl>
#include <modules/scoreboard.inl>
#include <modules/amxbans.inl>
#include <modules/csstatsx.inl>

#include <ezjson_gc>

new const PLUGIN[]  	= "Tsarapi"
new const VERSION[] 	= "0.0.1"
new const AUTHOR[]		= "ekke bea?"

const MAX_TOKEN_LEN = 64;
new const CONFIG_FILE_NAME[] = "tsarapi.cfg";
new const API_URL[] = "http://localhost:3000";

const Float:SEND_QUEUE_EVENTS_INTERVAL = 3.0;

new g_token[MAX_TOKEN_LEN], g_pluginId;

enum _:STRUCT_QUEUE_EVENT {
	QUEUE_EVENT_NAME[32],
	EzJSON:QUEUE_EVENT_DATA,
	QUEUE_EVENT_HAPPENED_AT
}
new Array:g_arrQueueEvents;

new TASKID_QUEUE_EVENTS_WORKER;

public plugin_init() {
	g_pluginId = register_plugin(PLUGIN, VERSION, AUTHOR, "https://tsarvar.com");

	g_arrQueueEvents = ArrayCreate(STRUCT_QUEUE_EVENT);

	module_chat_init();
	module_amxbans_init();
	module_csstatsx_init();
	module_killfeed_init();
	module_scoreboard_init();

	queue_events_worker_init();

	TASKID_QUEUE_EVENTS_WORKER = generate_task_id();
}

public plugin_cfg() {
	bind_pcvar_string(register_cvar("tsarapi_token", "", FCVAR_PROTECTED), g_token, charsmax(g_token));

	module_chat_cfg();
	module_amxbans_cfg();
	module_csstatsx_cfg();
	module_killfeed_cfg();
	module_scoreboard_cfg();

	config_exec();
}

config_exec() {
	new cfgDir[PLATFORM_MAX_PATH]; 
	get_configsdir(cfgDir, charsmax(cfgDir));

	new configFilePath[PLATFORM_MAX_PATH];
	formatex(configFilePath, charsmax(configFilePath), "%s/%s", cfgDir, CONFIG_FILE_NAME);

	if(file_exists(configFilePath)) 
		server_cmd("exec ^"%s^"", configFilePath);
}

queue_event_emit(name[], EzJSON:_MOVE_data, replaceIfHandler[] = "", data[] = "") {
	new entry[STRUCT_QUEUE_EVENT], index = -1;

	if(strlen(replaceIfHandler) > 0) {
		new Array:arrEventsOfType = ArrayCreate();
		for(new i; i < ArraySize(g_arrQueueEvents); i++) {
			ArrayGetArray(g_arrQueueEvents, i, entry)
			if(!equal(entry[QUEUE_EVENT_NAME], name)) continue;

			ArrayPushCell(arrEventsOfType, entry[QUEUE_EVENT_DATA]);
		}

		if(ArraySize(arrEventsOfType) > 0) {
			new ret, frwd = CreateOneForward(g_pluginId, replaceIfHandler, FP_CELL, FP_ARRAY);
			ExecuteForward(frwd, ret, arrEventsOfType, data);
			DestroyForward(frwd);

			if(ret >= -1 && ret < ArraySize(g_arrQueueEvents)) index = ret
			else abort(AMX_ERR_BOUNDS, "Returned an invalid index %d for events array", ret);
		}	
	}
	
	copy(entry[QUEUE_EVENT_NAME], charsmax(entry[QUEUE_EVENT_NAME]), name);
	entry[QUEUE_EVENT_DATA] = _MOVE_data;
	entry[QUEUE_EVENT_HAPPENED_AT] = get_systime();

	if(index >= 0)
		ArraySetArray(g_arrQueueEvents, index, entry);

	return ArrayPushArray(g_arrQueueEvents, entry);
}

public queue_events_worker_init() {
	set_task(SEND_QUEUE_EVENTS_INTERVAL, "@task_send_queue_events", TASKID_QUEUE_EVENTS_WORKER, .flags = "b");
}

@task_send_queue_events() {
	if(ArraySize(g_arrQueueEvents) > 0) {
		queue_events_http_post(g_arrQueueEvents);
		ArrayClear(g_arrQueueEvents);
	}
}

queue_events_http_post(Array:events) {
	new EzHttpOptions:ezhttpOpt = ezhttp_create_options();
	ezhttp_option_set_header(ezhttpOpt, "Content-Type", "application/json");
	ezhttp_option_set_timeout(ezhttpOpt, 3000);

	new EzJSON_GC:gc = ezjson_gc_init();

	new EzJSON:root = ezjson_init_object();
	gc += root;

	ezjson_object_set_string(root, "token", g_token);

	new EzJSON:items = ezjson_init_array();
	gc += items;

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

	ezjson_object_set_value(root, "items", items);
	ezhttp_option_set_body_from_json(ezhttpOpt, root);

	ezhttp_post(API_URL, "@on_queue_events_post_complete", ezhttpOpt);

	ezjson_gc_destroy(gc);
}

@on_queue_events_post_complete(EzHttpRequest:request_id) {
	if(ezhttp_get_error_code(request_id) != EZH_OK) {
		new error[64]; ezhttp_get_error_message(request_id, error, charsmax(error));
		abort(AMX_ERR_GENERAL, "http response error: %s", error);
		return
	}
}