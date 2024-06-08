#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static bool:isModuleEnabled 

public module_scoreboard_cfg() {
}

public module_scoreboard_init() {
	bind_pcvar_num(register_cvar("tsarapi_send_scoreboard", "", FCVAR_PROTECTED), isModuleEnabled);

	set_task(10.0, "@module_scoreboard_send_updates", generate_task_id(), .flags = "b");
}

@module_scoreboard_send_updates() {
	if(!isModuleEnabled) return;

	static plName[MAX_PLAYERS + 1][MAX_NAME_LENGTH + 1], plTeam[MAX_PLAYERS + 1], 
		plScore[MAX_PLAYERS + 1], plDeaths[MAX_PLAYERS + 1], playersCount

	new bool:shouldQueueSnap, count;

	for(new id = 1; id <= MaxClients; id++) {
		if(!is_user_connected(id)) continue;

		if(!equal(plName[id], get_player_name(id)) 
			|| plTeam[id] != get_user_team(id) 
			|| plScore[id] != get_player_score(id)
			|| plDeaths[id] != get_player_deaths(id)
		) {
			shouldQueueSnap = true;

			copy(plName[id], charsmax(plName[]), get_player_name(id));
			plTeam[id] = get_user_team(id);
			plScore[id] = get_player_score(id);
			plDeaths[id] = get_player_deaths(id);
		}
		count++;
	}

	if(shouldQueueSnap || count != playersCount)
		module_scoreboard_snap_and_queue();

	playersCount = count;
}

module_scoreboard_snap_and_queue() {
	new EzJSON:items = ezjson_init_array();

	for(new id = 1; id <= MaxClients; id++) {
		if(!is_user_connected(id)) continue;

		new EzJSON:object = ezjson_init_object();
		ezjson_object_add_player_props(object, id);
		ezjson_object_set_number(object, "score", get_player_score(id));
		ezjson_object_set_number(object, "deaths", get_player_deaths(id));

		ezjson_array_append_value(items, object);
	}

	queue_event_emit("scoreboard", items);
}