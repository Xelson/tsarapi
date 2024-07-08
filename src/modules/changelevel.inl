#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static cvarId;
static bool:isModuleEnabled

public module_changelevel_cfg() {
	config_observer_watch_cvar(
		cvarId, 
		"send_changelevel", 
		config_option_number
	);

	if(!isModuleEnabled) return;
	
	new EzJSON:root = ezjson_init_object();
	new mapName[32]; get_mapname(mapName, charsmax(mapName));
	ezjson_object_set_string(root, "mapname", mapName);
	
	queue_event_emit("changelevel", root);
}

public module_changelevel_init() {
	cvarId = register_cvar("tsarapi_send_changelevel", "", FCVAR_PROTECTED);
	bind_pcvar_num(cvarId, isModuleEnabled);
}