#include <amxmodx>
#include <amxmisc>
#include <sqlx>
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

new const CONFIG_FILE_NAME[] 	= "tsarapi.cfg";
new const API_URL[] 			= "http://localhost:3000";

new const TIMESTAMP_FORMAT[] 		= "%Y-%m-%d %H:%M:%S";
new const SCHEDULER_TABLE_NAME[] 	= "tsarapi_scheduled_tasks";

const Float:SEND_QUEUE_EVENTS_INTERVAL = 3.0;
const MAX_TOKEN_LEN = 64;

new g_token[MAX_TOKEN_LEN], g_pluginId;

public plugin_init() {
	g_pluginId = register_plugin(PLUGIN, VERSION, AUTHOR, "https://tsarvar.com");

	scheduler_worker_init();
	queue_events_worker_init();

	module_chat_init();
	module_amxbans_init();
	module_csstatsx_init();
	module_killfeed_init();
	module_scoreboard_init();
}

public plugin_cfg() {
	bind_pcvar_string(register_cvar("tsarapi_token", "", FCVAR_PROTECTED), g_token, charsmax(g_token));

	module_chat_cfg();
	module_amxbans_cfg();
	module_csstatsx_cfg();
	module_killfeed_cfg();
	module_scoreboard_cfg();

	config_exec();
	sql_init();
}

new g_sqlHost[64], g_sqlUser[64], g_sqlPassword[128], g_sqlDatabase[64];
new Handle:g_sqlTuple;

static sql_init() {
	new cvarHost = register_cvar("tsarapi_sql_host", "", FCVAR_PROTECTED)
	new cvarUser = register_cvar("tsarapi_sql_user", "", FCVAR_PROTECTED)
	new cvarPass = register_cvar("tsarapi_sql_pass", "", FCVAR_PROTECTED)
	new cvarDb = register_cvar("tsarapi_sql_db", "", FCVAR_PROTECTED)

	bind_pcvar_string(cvarHost, g_sqlHost, charsmax(g_sqlHost));
	bind_pcvar_string(cvarUser, g_sqlUser, charsmax(g_sqlUser));
	bind_pcvar_string(cvarPass, g_sqlPassword, charsmax(g_sqlPassword));
	bind_pcvar_string(cvarDb, g_sqlDatabase, charsmax(g_sqlDatabase));

	hook_cvar_change(cvarHost, "@on_sql_credential_changed");
	hook_cvar_change(cvarUser, "@on_sql_credential_changed");
	hook_cvar_change(cvarPass, "@on_sql_credential_changed");
	hook_cvar_change(cvarDb, "@on_sql_credential_changed");

	set_task(0.1, "@task_sql_tuple_init", generate_task_id());
}

@on_sql_credential_changed() {
	log_amx("Detected a change of the SQL credentials. Recreating the tuple.");
	sql_connection_make_tuple();
}

@task_sql_tuple_init() {
	sql_connection_make_tuple();
}

static sql_connection_make_tuple() {
	if(g_sqlTuple != Empty_Handle) SQL_FreeHandle(g_sqlTuple);
	g_sqlTuple = SQL_MakeDbTuple(g_sqlHost, g_sqlUser, g_sqlPassword, g_sqlDatabase);

	scheduler_worker_sql_tuple_init();
}

sql_make_query(const query[], const handler[], const data[] = "", len = 0) {
	ASSERT(g_sqlTuple, "Trying to send SQL query without connection tuple");

	SQL_ThreadQuery(g_sqlTuple, query, handler, data, len);
}

static config_exec() {
	new cfgDir[PLATFORM_MAX_PATH]; 
	get_configsdir(cfgDir, charsmax(cfgDir));

	new configFilePath[PLATFORM_MAX_PATH];
	formatex(configFilePath, charsmax(configFilePath), "%s/%s", cfgDir, CONFIG_FILE_NAME);

	if(file_exists(configFilePath)) 
		server_cmd("exec ^"%s^"", configFilePath);
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

static queue_events_http_post(Array:events) {
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

enum _:STRUCT_SCHEDULER_TASK {
	SCHEDULER_TASK_NAME[32],
	EzJSON:SCHEDULER_TASK_STATE,
	SCHEDULER_TASK_EXECUTING_AT,
	SCHEDULER_TASK_TIMER_ID,
	SCHEDULER_TASK_HANDLER
}
new Array:g_arrSheduledTasks

static scheduler_worker_init() {
	g_arrSheduledTasks = ArrayCreate(STRUCT_SCHEDULER_TASK);
}

scheduler_task_define(const name[], const initialState[], const executingHandler[], executingAt) {
	ASSERT(
		ArrayFindString(g_arrSheduledTasks, name) == -1, 
		"Trying to duplicate declaration of the %s task", name
	);

	new task[STRUCT_SCHEDULER_TASK];
	copy(task[SCHEDULER_TASK_NAME], charsmax(task[SCHEDULER_TASK_NAME]), name);

	new EzJSON:root = ezjson_parse(initialState);
	ASSERT(root != EzInvalid_JSON, "Invalid initial state on the %s task declaration", name);

	task[SCHEDULER_TASK_STATE] = root;
	task[SCHEDULER_TASK_EXECUTING_AT] = executingAt;
	task[SCHEDULER_TASK_TIMER_ID] = 0;
	task[SCHEDULER_TASK_HANDLER] = CreateOneForward(g_pluginId, executingHandler, FP_CELL)

	scheduler_task_cache_merge(task);
}

scheduler_tasks_count() {
	return ArraySize(g_arrSheduledTasks);
}

scheduler_task_is_valid_id(taskId) {
	return taskId >= 0 && taskId < scheduler_tasks_count();
}

scheduler_task_get_data(taskId, task[STRUCT_SCHEDULER_TASK]) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArrayGetArray(g_arrSheduledTasks, taskId, task);
}

scheduler_task_sql_commit_changes(taskId) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	new serializedState[512];
	ezjson_serial_to_string(task[SCHEDULER_TASK_STATE], serializedState, charsmax(serializedState));
	SQL_QuoteString(Empty_Handle, serializedState, charsmax(serializedState), serializedState);

	new serializedTime[32];
	format_time(serializedTime, charsmax(serializedTime), TIMESTAMP_FORMAT, task[SCHEDULER_TASK_EXECUTING_AT]);

	sql_make_query(
		_fmt("INSERT INTO `%s` VALUES ('%s', '%s', '%s') \
			ON DUPLICATE KEY UPDATE state = '%s', executing_at = '%s'",
			SCHEDULER_TABLE_NAME, 
			task[SCHEDULER_TASK_NAME], serializedState, serializedTime,
			serializedState, serializedTime
		),
		"@on_scheduler_worker_sql_commit_changes"
	);
}

@on_scheduler_worker_sql_commit_changes(failstate, Handle:query, error[]) {
	ASSERT(failstate == TQUERY_SUCCESS, error);
}

scheduler_task_cache_merge(task[STRUCT_SCHEDULER_TASK]) {
	new index = ArrayFindString(g_arrSheduledTasks, task[SCHEDULER_TASK_NAME]);
	if(index != -1) ArraySetArray(g_arrSheduledTasks, index, task);
	else ArrayPushArray(g_arrSheduledTasks, task);
}

static scheduler_worker_sql_tuple_init() {
 	sql_make_query(
		_fmt("CREATE TABLE IF NOT EXISTS `%s` ( \ 
			name VARCHAR(32) PRIMARY KEY NOT NULL, \
			state TEXT NOT NULL DEFAULT '{}', \
			executing_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP \
		)", SCHEDULER_TABLE_NAME),
		"@on_scheduler_worker_sql_tables_created"
	);

	sql_make_query(
		_fmt("SELECT * FROM `%s` WHERE 1", SCHEDULER_TABLE_NAME),
		"@on_scheduler_worker_sql_sync_cache"
	);
}

@on_scheduler_worker_sql_tables_created(failstate, Handle:query, error[]) {
	ASSERT(failstate == TQUERY_SUCCESS, error);
}

@on_scheduler_worker_sql_sync_cache(failstate, Handle:query, error[]) {
	scheduler_tasks_cache_merge_from_sql(query);
	scheduler_tasks_schedule_executing();

	ASSERT(failstate == TQUERY_SUCCESS, error);
}

static scheduler_tasks_cache_merge_from_sql(Handle:query) {
	new buffer[1024];
	enum { field_name, field_state, field_executing_at };

	for(new task[STRUCT_SCHEDULER_TASK]; SQL_MoreResults(query); SQL_NextRow(query)) {
		// Логика на удаление лишних записей из БД, если соответствующих записей не оказалось в кеше (не декларированы)
		// Нужно отменить мёрж, если таска в кеше нет

		// В то же время, если в базе нет какого-то таска из кеша (они были декларированы до того, как были сохранены в БД),
		// мы делаем scheduler_task_sql_commit_changes для них, чтобы синхронизировать с БД

		SQL_ReadResult(query, field_name, task[SCHEDULER_TASK_NAME], charsmax(task[SCHEDULER_TASK_NAME]));
		SQL_ReadResult(query, field_state, buffer, charsmax(buffer));
		
		new EzJSON:root = ezjson_parse(buffer);
		if(root == EzInvalid_JSON) root = ezjson_init_object();
		task[SCHEDULER_TASK_STATE] = root;

		SQL_ReadResult(query, field_executing_at, buffer, charsmax(buffer));
		task[SCHEDULER_TASK_EXECUTING_AT] = parse_time(buffer, TIMESTAMP_FORMAT);

		scheduler_task_cache_merge(task);
	}
}

static scheduler_tasks_schedule_executing() {
	for(new i, task[STRUCT_SCHEDULER_TASK]; i < ArraySize(g_arrSheduledTasks); i++) {
		ArrayGetArray(g_arrSheduledTasks, i, task);

		new data[1]; data[0] = i;

		task[SCHEDULER_TASK_TIMER_ID] = set_task(
			float(task[SCHEDULER_TASK_EXECUTING_AT] - get_systime()), 
			"@task_scheduler_execute", 
			generate_task_id(),
			data, sizeof(data)
		);
	}
}

@task_scheduler_execute(data[1]) {
	new taskId = data[0], task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	new ret;
	ExecuteForward(task[SCHEDULER_TASK_HANDLER], ret, taskId);
}