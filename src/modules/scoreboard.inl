#include <amxmodx>

static bool:isModuleEnabled 

public module_scoreboard_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_scoreboard", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_scoreboard_init() {
	
}