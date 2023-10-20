#include <amxmodx>

static bool:isModuleEnabled 

public module_csstatsx_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_csstatsx", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_csstatsx_init() {
	
}