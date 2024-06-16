#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>
#include <fakemeta>

static bool:isModuleEnabled 

public module_changelevel_cfg() {
	if(!isModuleEnabled) return;
	
	new EzJSON:root = ezjson_init_object();
	ezjson_object_set_string(root, "mapname", MapName);
	
	queue_event_emit("changelevel", root);
}

public module_changelevel_init() {
	bind_pcvar_num(register_cvar("tsarapi_send_changelevel", "", FCVAR_PROTECTED), isModuleEnabled);
}