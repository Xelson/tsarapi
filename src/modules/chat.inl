#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>

const MAX_CHAT_MSG_LEN = 192;

static cvarId;
static bool:isModuleEnabled;

public module_chat_cfg() {
	config_observer_watch_cvar(
		cvarId, 
		"send_chat", 
		config_option_number
	);
}

public module_chat_init() {
	cvarId = register_cvar("tsarapi_send_chat", "", FCVAR_PROTECTED);
	bind_pcvar_num(cvarId, isModuleEnabled);

	set_task(1.0, "@module_chat_register_clcmd", generate_task_id());
}

@module_chat_register_clcmd() {
	register_clcmd("say", "@module_chat_on_clcmd_say");
}

@module_chat_on_clcmd_say(id) {
	if(!isModuleEnabled)
		return;

	new message[MAX_CHAT_MSG_LEN];
	read_args(message, charsmax(message));
	remove_quotes(message);

	if(!strlen(message))
		return;

	module_chat_on_player_send_message(id, message);
}

module_chat_on_player_send_message(id, message[MAX_CHAT_MSG_LEN]) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_add_player_props(data, id);
	ezjson_object_set_string(data, "message", message);

	queue_event_emit("send_chat_message", data);
}