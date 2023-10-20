#include <amxmodx>

static bool:isModuleEnabled 

public module_amxbans_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_amxbans", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_amxbans_init() {
	
}