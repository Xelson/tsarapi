#include <amxmodx>

const MAX_WEAPON_NAME_LEN = 64;

static bool:isModuleEnabled 

public module_killfeed_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_killfeed", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_killfeed_init() {
	register_message(get_user_msgid("DeathMsg"), "@on_message_deathmsg")
}

static @on_message_deathmsg(victim, killer) {
	enum { arg_killer = 1, arg_victim, arg_headshot, arg_weapon_name };

	new killer = get_msg_arg_int(arg_killer);
	new victim = get_msg_arg_int(arg_victim);
	new headshot = get_msg_arg_int(arg_headshot);

	new weaponName[MAX_WEAPON_NAME_LEN];
	get_msg_arg_string(arg_weapon_name, weaponName, charsmax(weaponName));

	on_new_deathnotice(killer, victim, weaponName, bool:headshot);
}

static on_new_deathnotice(killer, victim, weaponName[MAX_WEAPON_NAME_LEN], bool:isHeadshot) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_set_string(data, "killer_name", fmt("%n", killer));
	ezjson_object_set_number(data, "killer_team", get_user_team(killer));
	ezjson_object_set_string(data, "victim_name", fmt("%n", victim));
	ezjson_object_set_number(data, "victim_team", get_user_team(victim));
	ezjson_object_set_string(data, "weapon_name", weaponName);
	ezjson_object_set_bool(data, "is_headshot", isHeadshot);

	queue_event_emit("death_notice", data);
}