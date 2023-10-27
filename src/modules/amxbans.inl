#include <amxmodx>
#include <sqlx>
#include <easy_http>
#include <tsarapi_util>

static bool:isModuleEnabled 

public module_amxbans_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_amxbans", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_amxbans_init() {
	scheduler_task_define(
		"amxbans_fetch",
		"{^"current_page^":0,^"max_pages^":10,^"limit^":1000}",
		"@module_amxbans_execute_task",
		"@module_amxbans_get_executing_time"
	)
}

@module_amxbans_get_executing_time() {
	return get_next_systime_in("1:44:00");
}

@module_amxbans_execute_task(taskId) {
	if(!isModuleEnabled) {
		scheduler_task_set_executing_at(taskId, scheduler_task_get_next_execution_time(taskId));
		scheduler_task_sql_commit_changes(taskId);
		return;
	}

	server_print("Emulation of a amxbans fetching activity...");
	fetch_table(taskId);
}

static fetch_table(taskId) {
	new EzJSON:st = scheduler_task_get_state(taskId)
	new page = ezjson_object_get_number(st, "current_page");
	new limit = ezjson_object_get_number(st, "limit");

	server_print("starting to fetch on page %d (%d items)", page, limit);
	
	new data[1]; data[0] = taskId;
	set_task(3.0, "@fetch_table_result", generate_task_id(), data, sizeof(data));
}

@fetch_table_result(data[1]) {
	new taskId = data[0];
	server_print("Got the data and sended it, at that moment we can update task state");

	new EzJSON:st = scheduler_task_get_state(taskId);
	new page = ezjson_object_get_number(st, "current_page");
	new maxPages = ezjson_object_get_number(st, "max_pages");

	if(page < maxPages) {
		const nextOperationTime = 3;
		ezjson_object_set_number(st, "current_page", page + 1);
		
		scheduler_task_set_state(taskId, st);
		scheduler_task_set_executing_at(taskId, get_systime() + nextOperationTime);
		
		server_print("There is pages to fetch, repeating operation in %d seconds", nextOperationTime);
	}
	else {
		ezjson_object_set_number(st, "current_page", 0);
		scheduler_task_set_executing_at(taskId, scheduler_task_get_next_execution_time(taskId));

		server_print("Fetching is done!");
	}

	scheduler_task_sql_commit_changes(taskId);
}

Handle:sql_make_bans_tuple() {
	static host[64], user[64], pass[64], db[64];

	if(cvar_exists("fb_sql_host")) {
		new configDir[PLATFORM_MAX_PATH]; 
		get_configsdir(configDir, charsmax(configDir));
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

	return SQL_MakeDbTuple(host, user, pass, db);
}