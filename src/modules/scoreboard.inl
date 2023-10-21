#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static bool:isModuleEnabled 

public module_scoreboard_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_scoreboard", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_scoreboard_init() {
	register_logevent("@module_scoreboard_on_round_end", 2, "1=Round_End");
	register_forward(FM_ClientDisconnect, "@module_scoreboard_on_client_disconnect", true); 

	set_task(3.0, "@module_scoreboard_check_updates", generate_task_id(), .flags = "b");
}

@module_scoreboard_check_updates() {
	on_check_updates();
}

@module_scoreboard_on_client_disconnect(id) {
	on_client_disconnect(id);
}

@module_scoreboard_on_round_end() {
	on_round_end();
}

static on_client_disconnect(id) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_set_number(data, "player_slotid", id);

	queue_event_emit("player_disconnected", data);
}

static on_round_end() {
	new EzJSON:data = ezjson_init_object();

	queue_event_emit("round_end", data);
}

static on_check_updates() {
	static plName[MAX_PLAYERS][MAX_NAME_LENGTH], plTeam[MAX_PLAYERS], 
		plScore[MAX_PLAYERS], plDeaths[MAX_PLAYERS]

	for(new id = 1; id <= MaxClients; id++) {
		if(!is_user_connected(id)) continue;

		if(!equal(plName[id], get_player_name(id)) 
			|| plTeam[id] != get_user_team(id) 
			|| plScore[id] != get_player_score(id)
			|| plDeaths[id] != get_player_team(id)
		) {
			on_player_state_changed(id);

			copy(plName[id], charsmax(plName[]), get_player_name(id));
			plTeam[id] = get_user_team(id);
			plScore[id] = get_player_score(id);
			plDeaths[id] = get_player_team(id);
		}
	}
}

static on_player_state_changed(id) {
	new EzJSON:object = ezjson_init_object();
	ezjson_object_add_player_props(object, id);
	ezjson_object_set_number(object, "player_score", get_player_score(id));
	ezjson_object_set_number(object, "player_deaths", get_player_deaths(id));

	new data[1]; data[0] = id;

	queue_event_emit(
		"player_state_changed", object, 
		.replaceIfHandler = "@module_scoreboard_replace_event_if",
		.data = data
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