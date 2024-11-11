#include <amxmodx>
#include <amxmisc>
#include <easy_http>
#include <nvault>

enum _:STRUCT_SCHEDULER_TASK {
	SCHEDULER_TASK_NAME[32], // DONT CHANGE THE POSITION
	SCHEDULER_TASK_ID,
	EzJSON:SCHEDULER_TASK_STATE,
	SCHEDULER_TASK_TIMER_ID,
	SCHEDULER_TASK_HANDLER,
	SCHEDULER_TASK_EXECUTING_AT,
	SCHEDULER_TASK_GET_EXEC_TIME
}
new Array:g_arrScheduledTasks

static vault

static scheduler_worker_init() {
	g_arrScheduledTasks = ArrayCreate(STRUCT_SCHEDULER_TASK);

	register_srvcmd("tsarapi_task_start", "@on_srvcmd_task_start");
}

static scheduler_worker_cfg() {
	vault = nvault_open("tsarapi_scheduler_tasks");
	if(vault != INVALID_HANDLE)
		scheduler_tasks_cache_merge_from_vault();
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
	return ArraySize(g_arrScheduledTasks);
}

scheduler_task_is_valid_id(taskId) {
	return taskId >= 0 && taskId < scheduler_tasks_count();
}

scheduler_task_get_data(taskId, task[STRUCT_SCHEDULER_TASK]) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArrayGetArray(g_arrScheduledTasks, taskId, task);
}

scheduler_task_get_name(taskId, dest[], len) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArrayGetString(g_arrScheduledTasks, taskId, dest, len);
}

static scheduler_task_set_data(taskId, task[STRUCT_SCHEDULER_TASK]) {
	ASSERT(scheduler_task_is_valid_id(taskId), "Invalid task id");

	ArraySetArray(g_arrScheduledTasks, taskId, task);
}

static scheduler_task_push_data(task[STRUCT_SCHEDULER_TASK]) {
	return ArrayPushArray(g_arrScheduledTasks, task);
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
	return ArrayFindString(g_arrScheduledTasks, name);
}

scheduler_task_set_executing_at(taskId, executingAt) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	task[SCHEDULER_TASK_EXECUTING_AT] = executingAt;
	_scheduler_task_timer_continue(task);

	scheduler_task_set_data(taskId, task);
}

scheduler_task_commit_changes(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	new serializedState[512];
	ezjson_serial_to_string(task[SCHEDULER_TASK_STATE], serializedState, charsmax(serializedState));

	new serializedTime[32];
	format_timestamp(serializedTime, charsmax(serializedTime), task[SCHEDULER_TASK_EXECUTING_AT]);

	nvault_pset(vault, task[SCHEDULER_TASK_NAME], _fmt("^"%s^"", serializedTime))
	nvault_pset(vault, _fmt("%s_state",task[SCHEDULER_TASK_NAME]), serializedState)
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

static scheduler_tasks_cache_merge_from_vault() {
	enum _:FIELDS { arg_executing_at };

	new taskId, value[256], args[FIELDS][256]
	new Array:arrFoundTaskIds = ArrayCreate();

	for(taskId = 0;
		taskId < scheduler_tasks_count(); 
		taskId++
	) {
		new task[STRUCT_SCHEDULER_TASK], timestamp
		scheduler_task_get_data(taskId, task);

		if(!nvault_lookup(vault, task[SCHEDULER_TASK_NAME], value, charsmax(value), timestamp))
			continue;

		parse(value, args[arg_executing_at], charsmax(args[]));
		task[SCHEDULER_TASK_EXECUTING_AT] = parse_timestamp(args[arg_executing_at]);

		if(!nvault_lookup(vault, _fmt("%s_state",task[SCHEDULER_TASK_NAME]), value, charsmax(value), timestamp))
			continue;

		new EzJSON:root = ezjson_parse(value);
		if(root == EzInvalid_JSON) root = ezjson_init_object();
		task[SCHEDULER_TASK_STATE] = root;

		scheduler_task_cache_merge(task);
		_scheduler_task_timer_continue(task);

		ArrayPushCell(arrFoundTaskIds, taskId);

		_scheduler_task_write_to_console(task);
	}

	for(taskId = 0; taskId < scheduler_tasks_count(); taskId++) {
		if(ArrayFindValue(arrFoundTaskIds, taskId) == -1) {
			scheduler_task_commit_changes(taskId);
			scheduler_task_timer_continue(taskId);

			scheduler_task_write_to_console(taskId);
		}
	}

	ArrayDestroy(arrFoundTaskIds);
}

scheduler_task_write_to_console(taskId) {
	new task[STRUCT_SCHEDULER_TASK];
	scheduler_task_get_data(taskId, task);

	_scheduler_task_write_to_console(task);
}	

_scheduler_task_write_to_console(task[STRUCT_SCHEDULER_TASK]) {
	new time[32]; format_time(time, charsmax(time), "%d/%m/%Y - %H:%M:%S", task[SCHEDULER_TASK_EXECUTING_AT]);
	log_amx("Task %s executing at %s", task[SCHEDULER_TASK_NAME], time);
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

	log_amx("Executing task %s...", task[SCHEDULER_TASK_NAME]);
}