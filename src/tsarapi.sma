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
new const CONFIG_FILE_NAME[] = "tsarapi.cfg"

new g_token[MAX_TOKEN_LEN]

enum _:STRUCT_QUEUE_EVENT {
	QUEUE_EVENT_NAME[32],
	EzJSON:QUEUE_EVENT_DATA
}
new Array:g_arrQueueEvents

public plugin_init() {
	register_plugin(PLUGIN, VERSION, AUTHOR, "https://tsarvar.com");

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

public queue_event_emit(name[], EzJSON:_MOVE_data) {
	new entry[STRUCT_QUEUE_EVENT];
	copy(entry[QUEUE_EVENT_NAME], charsmax(entry[QUEUE_EVENT_NAME]), name);
	entry[QUEUE_EVENT_DATA] = _MOVE_data;

	return ArrayPushArray(g_arrQueueEvents, entry);
}