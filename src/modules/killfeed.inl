#include <amxmodx>

static bool:isModuleEnabled 

public module_killfeed_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_killfeed", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_killfeed_init() {
	
}