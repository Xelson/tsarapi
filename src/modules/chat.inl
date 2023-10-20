#include <amxmodx>

static bool:isModuleEnabled 

public module_chat_cfg() {
	bind_pcvar_num(register_cvar("tsarapi_send_chat", "", FCVAR_PROTECTED), isModuleEnabled);
}

public module_chat_init() {
	
}