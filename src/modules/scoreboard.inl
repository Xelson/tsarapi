#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static cvarId
static bool:isModuleEnabled
static g_teamScore[2];

public module_scoreboard_cfg() {
	config_observer_watch_cvar(
		cvarId, 
		"send_scoreboard", 
		config_option_number
	);
}

public module_scoreboard_init() {
	cvarId = register_cvar("tsarapi_send_scoreboard", "", FCVAR_PROTECTED)
	bind_pcvar_num(cvarId, isModuleEnabled);

	register_event("TeamScore", "event_teamscore", "a");

	set_task(10.0, "@module_scoreboard_send_updates", generate_task_id(), .flags = "b");
}

public event_teamscore() {
	new team[2]; read_data(1, team, charsmax(team));
	new teamIndex = (team[0] == 'C') ? 1 : 0;
	new score = read_data(2);

	if(score != g_teamScore[teamIndex]) {
		g_teamScore[teamIndex] = score;
		module_scoreboard_snap_and_queue();
	}
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
	new EzJSON:root = ezjson_init_object();
	new EzJSON:items = ezjson_init_array();

	for(new id = 1; id <= MaxClients; id++) {
		if(!is_user_connected(id)) continue;

		new EzJSON:object = ezjson_init_object();
		ezjson_object_add_player_props(object, id);
		ezjson_object_set_number(object, "score", get_player_score(id));
		ezjson_object_set_number(object, "deaths", get_player_deaths(id));

		ezjson_array_append_value(items, object);
	}

	ezjson_object_set_value(root, "players", items);
	ezjson_object_set_number(root, "score_t", g_teamScore[0]);
	ezjson_object_set_number(root, "score_ct", g_teamScore[1]);

	queue_event_emit(
		"scoreboard", root,
		.replaceIfHandler = "@module_scoreboard_replace_event_if"
	);
}

@module_scoreboard_replace_event_if(Array:events) {
	return 0; // возвращаем первую позицию т.к нам нужно отправлять только свежие данные из очереди
}