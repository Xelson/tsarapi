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

	sql_init();

	config_exec();

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
	ASSERT(ezhttp_get_error_code(request_id) == EZH_OK, "http response error: %s", error);
}

enum _:STRUCT_SCHEDULER_TASK {
	SCHEDULER_TASK_NAME[32], // DONT CHANGE THE POSITION
	SCHEDULER_TASK_ID,
	EzJSON:SCHEDULER_TASK_STATE,
	SCHEDULER_TASK_TIMER_ID,
	SCHEDULER_TASK_HANDLER,
	SCHEDULER_TASK_EXECUTING_AT,
	SCHEDULER_TASK_GET_EXEC_TIME
}
new Array:g_arrSheduledTasks

static scheduler_worker_init() {
	g_arrSheduledTasks = ArrayCreate(STRUCT_SCHEDULER_TASK);
}

scheduler_task_define(
	const name[], const initialState[], 
	const executingHandler[], const getExecutingTimeHandler[]
) {
	ASSERT(
		scheduler_task_id_get_by_name(name) == -1,
		"Trying to duplicate declaration of the %s task", name
	);

	new task[STRUCT_SCHEDULER_TASK];
	copy(task[SCHEDULER_TASK_NAME], charsmax(task[SCHEDULER_TASK_NAME]), name);

	new EzJSON:root = ezjson_parse(initialState);
	ASSERT(root != EzInvalid_JSON, "Invalid initial state on the %s task declaration", name);

	task[SCHEDULER_TASK_ID] = scheduler_tasks_count();
	task[SCHEDULER_TASK_STATE] = root;
	task[SCHEDULER_TASK_TIMER_ID] = 0;
	task[SCHEDULER_TASK_HANDLER] = CreateOneForward(g_pluginId, executingHandler, FP_CELL);
	task[SCHEDULER_TASK_GET_EXEC_TIME] = CreateOneForward(g_pluginId, getExecutingTimeHandler);
	task[SCHEDULER_TASK_EXECUTING_AT] = _scheduler_task_get_next_execution_time(task);

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

static scheduler_task_set_data(taskId, task[STRUCT_SCHEDULER_TASK]) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArraySetArray(g_arrSheduledTasks, taskId, task);
}

static scheduler_task_push_data(task[STRUCT_SCHEDULER_TASK]) {
	return ArrayPushArray(g_arrSheduledTasks, task);
}

EzJSON:scheduler_task_get_state(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);
	return task[SCHEDULER_TASK_STATE];
}

scheduler_task_set_state(taskId, EzJSON:st) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);
	task[SCHEDULER_TASK_STATE] = st;
	scheduler_task_set_data(taskId, task);
}

scheduler_task_get_executing_at(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	return task[SCHEDULER_TASK_EXECUTING_AT];
}

scheduler_task_id_get_by_name(const name[]) {
	return ArrayFindString(g_arrSheduledTasks, name);
}

scheduler_task_set_executing_at(taskId, executingAt) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	task[SCHEDULER_TASK_EXECUTING_AT] = executingAt;
	_scheduler_task_continue_executing(task);

	scheduler_task_set_data(taskId, task);
}

scheduler_task_sql_commit_changes(taskId) {
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

static scheduler_task_cache_merge(task[STRUCT_SCHEDULER_TASK]) {
	new taskId = scheduler_task_id_get_by_name(task[SCHEDULER_TASK_NAME]);
	if(taskId != -1) scheduler_task_set_data(taskId, task);
	else taskId = scheduler_task_push_data(task);
	return taskId;
}

scheduler_task_get_next_execution_time(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	return _scheduler_task_get_next_execution_time(task);
}

static _scheduler_task_get_next_execution_time(task[STRUCT_SCHEDULER_TASK]) {
	new time;
	ExecuteForward(task[SCHEDULER_TASK_GET_EXEC_TIME], time);
	return time;
}

static scheduler_worker_sql_tuple_init() {
 	sql_make_query(
		_fmt("CREATE TABLE IF NOT EXISTS `%s` ( \ 
			name VARCHAR(32) PRIMARY KEY NOT NULL, \
			state TEXT NOT NULL, \
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

	ASSERT(failstate == TQUERY_SUCCESS, error);
}

static scheduler_tasks_cache_merge_from_sql(Handle:query) {
	new buffer[1024];
	enum { field_name, field_state, field_executing_at };

	new Array:arrCachedTaskIds = ArrayCreate();

	for(new task[STRUCT_SCHEDULER_TASK]; SQL_MoreResults(query); SQL_NextRow(query)) {
		SQL_ReadResult(query, field_name, buffer, charsmax(buffer));

		new taskId = scheduler_task_id_get_by_name(buffer);
		if(taskId == -1) {
			// Может быть добавить код для чистки устаревших тасков	
			continue;
		}

		scheduler_task_get_data(taskId, task);
		SQL_ReadResult(query, field_state, buffer, charsmax(buffer));
		
		new EzJSON:root = ezjson_parse(buffer);
		if(root == EzInvalid_JSON) root = ezjson_init_object();
		task[SCHEDULER_TASK_STATE] = root;

		SQL_ReadResult(query, field_executing_at, buffer, charsmax(buffer));
		task[SCHEDULER_TASK_EXECUTING_AT] = parse_time(buffer, TIMESTAMP_FORMAT);

		scheduler_task_cache_merge(task);
		_scheduler_task_continue_executing(task);

		ArrayPushCell(arrCachedTaskIds, taskId);
	}

	for(new taskId; taskId < scheduler_tasks_count(); taskId++) {
		if(ArrayFindValue(arrCachedTaskIds, taskId) == -1) {
			scheduler_task_sql_commit_changes(taskId);
			scheduler_task_continue_executing(taskId);
		}
	}

	ArrayDestroy(arrCachedTaskIds);
}

scheduler_task_continue_executing(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	_scheduler_task_continue_executing(task);

	scheduler_task_set_data(taskId, task);
}

static _scheduler_task_continue_executing(task[STRUCT_SCHEDULER_TASK]) {
	remove_task(task[SCHEDULER_TASK_TIMER_ID]);

	task[SCHEDULER_TASK_TIMER_ID] = set_task(
		float(task[SCHEDULER_TASK_EXECUTING_AT] - get_systime()), 
		"@task_scheduler_execute", 
		generate_task_id(),
		task, sizeof(task)
	);
}

@task_scheduler_execute(task[STRUCT_SCHEDULER_TASK]) {
	new ret;
	ExecuteForward(task[SCHEDULER_TASK_HANDLER], ret, task[SCHEDULER_TASK_ID]);
}