#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static bool:isModuleEnabled 

public module_scoreboard_cfg() {
}

public module_scoreboard_init() {
	bind_pcvar_num(register_cvar("tsarapi_send_scoreboard", "", FCVAR_PROTECTED), isModuleEnabled);

	register_logevent("@module_scoreboard_on_round_end", 2, "1=Round_End");
	register_forward(FM_ClientDisconnect, "@module_scoreboard_on_client_disconnect", true); 

	set_task(3.0, "@module_scoreboard_check_updates", generate_task_id(), .flags = "b");
}

@module_scoreboard_check_updates() {
	if(!isModuleEnabled) return;

	static plName[MAX_PLAYERS + 1][MAX_NAME_LENGTH + 1], plTeam[MAX_PLAYERS + 1], 
		plScore[MAX_PLAYERS + 1], plDeaths[MAX_PLAYERS + 1]

	for(new id = 1; id <= MaxClients; id++) {
		if(!is_user_connected(id)) continue;

		if(!equal(plName[id], get_player_name(id)) 
			|| plTeam[id] != get_user_team(id) 
			|| plScore[id] != get_player_score(id)
			|| plDeaths[id] != get_player_deaths(id)
		) {
			module_scoreboard_on_player_state_changed(id);

			copy(plName[id], charsmax(plName[]), get_player_name(id));
			plTeam[id] = get_user_team(id);
			plScore[id] = get_player_score(id);
			plDeaths[id] = get_player_deaths(id);
		}
	}
}

@module_scoreboard_on_client_disconnect(id) {
	if(!isModuleEnabled) return;

	new EzJSON:data = ezjson_init_object();
	ezjson_object_set_number(data, "player_slotid", id);

	queue_event_emit("player_disconnected", data);
}

@module_scoreboard_on_round_end() {
	if(!isModuleEnabled) return;

	new EzJSON:data = ezjson_init_object();
	queue_event_emit("round_end", data);
}

module_scoreboard_on_player_state_changed(id) {
	new EzJSON:object = ezjson_init_object();
	ezjson_object_add_player_props(object, id);
	ezjson_object_set_number(object, "player_score", get_player_score(id));
	ezjson_object_set_number(object, "player_deaths", get_player_deaths(id));

	new data[1]; data[0] = id;

	queue_event_emit(
		"player_state_changed", object, 
		.replaceIfHandler = "@module_scoreboard_replace_event_if",
		.data = data, .dataLen = sizeof(data)
	);
}

@module_scoreboard_replace_event_if(Array:events, data[1]) {
	new id = data[0];

	for(new i, EzJSON:eventDataObj; i < ArraySize(events); i++) {
		eventDataObj = ArrayGetCell(events, i);
		if(ezjson_object_get_number(eventDataObj, "player_slotid") == id)
			return i;
	}

	return -1;
}