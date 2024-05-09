#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static bool:isModuleEnabled 

public module_changelevel_cfg() {
	if(!isModuleEnabled) return;
	
	new EzJSON:mapName = ezjson_init_string(MapName);
	queue_event_emit("changelevel", mapName);
}

public module_changelevel_init() {
	bind_pcvar_num(register_cvar("tsarapi_send_changelevel", "", FCVAR_PROTECTED), isModuleEnabled);
}