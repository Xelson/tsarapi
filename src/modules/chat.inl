#include <amxmodx>
#include <easy_http>

const MAX_CHAT_MSG_LEN = 192;

static bool:isModuleEnabled;

public module_chat_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_chat", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_chat_init() {
	register_clcmd("say", "clcmd_say");
	register_clcmd("say_team", "@clcmd_say_team");
}

static @clcmd_say(id) {
	if(!isModuleEnabled) return;

	new message[MAX_CHAT_MSG_LEN];
	read_args(message, charsmax(message));
	remove_quotes(message);

	on_player_send_message(id, message);
}

static @clcmd_say_team(id) {
	if(!isModuleEnabled) return;
	
	new message[MAX_CHAT_MSG_LEN];
	read_args(message, charsmax(message));
	remove_quotes(message);

	on_player_send_message(id, message, true);
}

static on_player_send_message(id, message[MAX_CHAT_MSG_LEN], bool:isTeamChat = false) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_set_string(data, "player_name", fmt("%n", id));
	ezjson_object_set_number(data, "player_team", get_user_team(id));
	ezjson_object_set_string(data, "message", message);
	ezjson_object_set_bool(data, "is_team_chat", isTeamChat);

	queue_event_emit("chat_message", data);
}