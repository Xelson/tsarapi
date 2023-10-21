#include <amxmodx>
#include <amxmisc>
#include <easy_http>

#include <modules/chat.inl>
#include <modules/killfeed.inl>
#include <modules/scoreboard.inl>
#include <modules/amxbans.inl>
#include <modules/csstatsx.inl>

new const PLUGIN[]  	= "Tsarapi"
new const VERSION[] 	= "0.0.1"
new const AUTHOR[]		= "ekke bea?"

const MAX_TOKEN_LEN = 64;
new const CONFIG_FILE_NAME[] = "tsarapi.cfg";

new g_token[MAX_TOKEN_LEN], g_pluginId;

enum _:STRUCT_QUEUE_EVENT {
	QUEUE_EVENT_NAME[32],
	EzJSON:QUEUE_EVENT_DATA,
	QUEUE_EVENT_HAPPENED_AT
}
new Array:g_arrQueueEvents

public plugin_init() {
	g_pluginId = register_plugin(PLUGIN, VERSION, AUTHOR, "https://tsarvar.com");

	g_arrQueueEvents = ArrayCreate(STRUCT_QUEUE_EVENT);

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