#include <amxmodx>
#include <amxmisc>
#include <sqlx>
#include <easy_http>

new const SCHEDULER_TABLE_NAME[] 	= "tsarapi_scheduled_tasks";

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

	register_srvcmd("tsarapi_task_start", "@on_srvcmd_task_start");
}

@on_srvcmd_task_start() {
	new tasksCount = scheduler_tasks_count();
	if(tasksCount == 0) {
		server_print("There are no registered tasks");
		return;
	}

	new taskName[32]; read_args(taskName, charsmax(taskName));
	new taskId = scheduler_task_id_get_by_name(taskName);

	if(taskId == -1) {
		server_print("Can't find '%s' task. Available tasks are:", taskName);
		for(new taskId = 0; taskId < tasksCount; taskId++) {
			scheduler_task_get_name(taskId, taskName, charsmax(taskName));
			server_print(taskName);
		}
		return;
	}

	server_print("Executing task '%s' (id: %d)...", taskName, taskId);
	scheduler_task_timer_stop(taskId);
	scheduler_task_execute(taskId);
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

scheduler_task_get_name(taskId, dest[], len) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArrayGetString(g_arrSheduledTasks, taskId, dest, len);
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

stock scheduler_task_get_executing_at(taskId) {
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
	_scheduler_task_timer_continue(task);

	scheduler_task_set_data(taskId, task);
}

scheduler_task_sql_commit_changes(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	new serializedState[512];
	ezjson_serial_to_string(task[SCHEDULER_TASK_STATE], serializedState, charsmax(serializedState));
	SQL_QuoteString(Empty_Handle, serializedState, charsmax(serializedState), serializedState);

	new serializedTime[32];
	format_timestamp(serializedTime, charsmax(serializedTime), task[SCHEDULER_TASK_EXECUTING_AT]);

	sql_make_query(
		_fmt("INSERT INTO `%s` VALUES ('%s', '%s', '%s', '%s') \
			ON DUPLICATE KEY UPDATE state = '%s', executing_at = '%s'",
			SCHEDULER_TABLE_NAME, 
			get_localhost(), task[SCHEDULER_TASK_NAME], serializedState, serializedTime,
			serializedState, serializedTime
		),
		"@on_scheduler_worker_sql_commit_changes"
	);
}

@on_scheduler_worker_sql_commit_changes(failstate, Handle:query, error[]) {
	if(failstate != TQUERY_SUCCESS) {
		log_sql_error(error);
		return;
	}
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
			address VARCHAR(24) NOT NULL, \
			name VARCHAR(32) NOT NULL, \
			state TEXT NOT NULL, \
			executing_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, \
			UNIQUE KEY unique_id (address, name) \
		)", SCHEDULER_TABLE_NAME),
		"@on_scheduler_worker_sql_tables_created"
	);

	sql_make_query(
		_fmt("SELECT * FROM `%s` WHERE address = '%s'", SCHEDULER_TABLE_NAME, get_localhost()),
		"@on_scheduler_worker_sql_sync_cache"
	);
}

@on_scheduler_worker_sql_tables_created(failstate, Handle:query, error[]) {
	if(failstate != TQUERY_SUCCESS) {
		log_sql_error(error);
		return;
	}
}

@on_scheduler_worker_sql_sync_cache(failstate, Handle:query, error[]) {
	scheduler_tasks_cache_merge_from_sql(query);

	if(failstate != TQUERY_SUCCESS) {
		log_sql_error(error);
		return;
	}
}

static scheduler_tasks_cache_merge_from_sql(Handle:query) {
	new buffer[1024];
	enum { field_address, field_name, field_state, field_executing_at };
	#pragma unused field_address

	new Array:arrCachedTaskIds = ArrayCreate();

	for(new task[STRUCT_SCHEDULER_TASK]; SQL_MoreResults(query); SQL_NextRow(query)) {
		SQL_ReadResult(query, field_name, buffer, charsmax(buffer));

		new taskId = scheduler_task_id_get_by_name(buffer);
		if(taskId == -1) {
			// maybe add code to clean up deleted tasks from the system
			server_print("%s not found in defined tasks", buffer);
			continue;
		}

		scheduler_task_get_data(taskId, task);
		SQL_ReadResult(query, field_state, buffer, charsmax(buffer));
		
		new EzJSON:root = ezjson_parse(buffer);
		if(root == EzInvalid_JSON) root = ezjson_init_object();
		task[SCHEDULER_TASK_STATE] = root;

		SQL_ReadResult(query, field_executing_at, buffer, charsmax(buffer));
		task[SCHEDULER_TASK_EXECUTING_AT] = parse_timestamp(buffer);

		scheduler_task_cache_merge(task);
		_scheduler_task_timer_continue(task);

		ArrayPushCell(arrCachedTaskIds, taskId);
	}

	for(new taskId; taskId < scheduler_tasks_count(); taskId++) {
		if(ArrayFindValue(arrCachedTaskIds, taskId) == -1) {
			scheduler_task_sql_commit_changes(taskId);
			scheduler_task_timer_continue(taskId);
		}
	}

	ArrayDestroy(arrCachedTaskIds);
}

scheduler_task_timer_continue(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	_scheduler_task_timer_continue(task);

	scheduler_task_set_data(taskId, task);
}

static _scheduler_task_timer_continue(task[STRUCT_SCHEDULER_TASK]) {
	_scheduler_task_timer_stop(task);

	task[SCHEDULER_TASK_TIMER_ID] = set_task(
		float(task[SCHEDULER_TASK_EXECUTING_AT] - get_systime()), 
		"@task_scheduler_execute", 
		generate_task_id(),
		task, sizeof(task)
	);
}

scheduler_task_timer_stop(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	_scheduler_task_timer_stop(task);
}

static _scheduler_task_timer_stop(task[STRUCT_SCHEDULER_TASK]) {
	remove_task(task[SCHEDULER_TASK_TIMER_ID]);
}

@task_scheduler_execute(task[STRUCT_SCHEDULER_TASK]) {
	_scheduler_task_execute(task);
}

scheduler_task_execute(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	_scheduler_task_execute(task);
}

static _scheduler_task_execute(task[STRUCT_SCHEDULER_TASK]) {
	new ret;
	ExecuteForward(task[SCHEDULER_TASK_HANDLER], ret, task[SCHEDULER_TASK_ID]);
}