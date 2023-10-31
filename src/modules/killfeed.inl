#include <amxmodx>
#include <tsarapi_util>

const MAX_WEAPON_NAME_LEN = 64;

static bool:isModuleEnabled 

public module_killfeed_cfg() {	
}

public module_killfeed_init() {
	register_message(get_user_msgid("DeathMsg"), "@module_killfeed_on_message_deathmsg");
	bind_pcvar_num(register_cvar("tsarapi_send_killfeed", "", FCVAR_PROTECTED), isModuleEnabled);
}

@module_killfeed_on_message_deathmsg(victim, killer) {
	if(!isModuleEnabled) return;
	
	enum { arg_killer = 1, arg_victim, arg_headshot, arg_weapon_name };

	new killer = get_msg_arg_int(arg_killer);
	new victim = get_msg_arg_int(arg_victim);
	new headshot = get_msg_arg_int(arg_headshot);

	new weaponName[MAX_WEAPON_NAME_LEN];
	get_msg_arg_string(arg_weapon_name, weaponName, charsmax(weaponName));

	module_killfeed_on_new_deathnotice(killer, victim, weaponName, bool:headshot);
}

module_killfeed_on_new_deathnotice(killer, victim, weaponName[MAX_WEAPON_NAME_LEN], bool:isHeadshot) {
	new EzJSON:data = ezjson_init_object();
	ezjson_object_add_player_props(data, killer, .prefix = "killer");
	ezjson_object_add_player_props(data, victim, .prefix = "victim");
	ezjson_object_set_string(data, "weapon_name", weaponName);
	ezjson_object_set_bool(data, "is_headshot", isHeadshot);

	queue_event_emit("new_death_notice", data);
}