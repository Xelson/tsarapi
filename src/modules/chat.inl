#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>

const MAX_CHAT_MSG_LEN = 192;

static bool:isModuleEnabled;

public module_chat_cfg() {
}

public module_chat_init() {
	bind_pcvar_num(register_cvar("tsarapi_send_chat", "", FCVAR_PROTECTED), isModuleEnabled);

	register_clcmd("say", "@module_chat_on_clcmd_say");
	register_clcmd("say_team", "@module_chat_on_clcmd_say_team");
}

@module_chat_on_clcmd_say(id) {
	if(!isModuleEnabled) return;

	new message[MAX_CHAT_MSG_LEN];
	read_args(message, charsmax(message));
	remove_quotes(message);

	on_player_send_message(id, message);
}

@module_chat_on_clcmd_say_team(id) {
	if(!isModuleEnabled) return;
	
	new message[MAX_CHAT_MSG_LEN];
	read_args(message, charsmax(message));
	remove_quotes(message);

	on_player_send_message(id, message, true);
}

static on_player_send_message(id, message[MAX_CHAT_MSG_LEN], bool:isTeamChat = false) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_add_player_props(data, id);
	ezjson_object_set_string(data, "message", message);
	ezjson_object_set_bool(data, "is_team_chat", isTeamChat);

	queue_event_emit("send_chat_message", data);
}