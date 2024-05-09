#if defined _tsarapi_utils_included
	#endinput
#endif
#define _tsarapi_utils_included

#include <amxmodx>
#include <easy_http>
#include <fakemeta>

stock get_player_name(id) {
	static name[MAX_NAME_LENGTH];
	get_user_name(id, name, charsmax(name));
	return name;
}

stock get_player_steamid(id) {
	static steamId[MAX_AUTHID_LENGTH];
	get_user_authid(id, steamId, charsmax(steamId));
	return steamId;
}

stock get_player_deaths(id) {
	return get_pdata_int(id, 444, 5);
}

stock get_player_score(id) {
	return get_user_frags(id);
}

stock get_player_team(id) {
	return get_user_team(id);
}

stock ezjson_object_add_player_props(EzJSON:object, id, prefix[] = "") {
	// ezjson_object_set_number(object, fmt("%sslotid", prefix), id);
	ezjson_object_set_string(object, fmt("%ssteamid", prefix), get_player_steamid(id));
	ezjson_object_set_string(object, fmt("%sname", prefix), get_player_name(id));
	ezjson_object_set_number(object, fmt("%steam", prefix), get_player_team(id));
}

stock generate_task_id(step = 1) {
	static id = 1;
	id += step;
	return id;
}

stock _fmt(format[], any:...) {
	static buffer[1024];
	vformat(buffer, charsmax(buffer), format, 2);
	return buffer;
}

stock ASSERT(any:predicate, err[], any:...) {
	new buffer[192]; vformat(buffer, charsmax(buffer), err, 3);
	if(!predicate) abort(AMX_ERR_NATIVE, buffer);
}

get_next_systime_in(const time[]) {
	static const SECONDS_IN_DAY = 86400;
	new targetTime = parse_time(time, "%H:%M:%S");
	
	return get_systime() > targetTime ? targetTime + SECONDS_IN_DAY : targetTime;
}