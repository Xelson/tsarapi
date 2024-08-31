#include <amxmodx>

#define PLATFORM_MAX_PATH 256
#define MAX_IP_WITH_PORT_LENGTH 22
#define MAX_NAME_LENGTH 32
#define MAX_AUTHID_LENGTH 64
#define MAX_PLAYERS 32

#include <tsarapi_util>

new MaxClients;

enum _:_BINDED_CVAR {
	_BC_POINTER,
	_BC_VARIABLE_ADDRESS,
	_BC_VARIABLE_SIZE,
	bool:_BC_VARIABLE_IS_FLOAT
}
static Array:bindedCvars;

enum _:_HOOCKED_CVAR {
	_HC_POINTER,
	_HC_PREVIOUS_VALUE[512],
	_HC_CHANGE_CALLBACK
}
static Array:hookedCvars;

static g_pluginId

amxx_182_compact_init(pluginId) {
	g_pluginId = pluginId

	MaxClients = get_maxplayers();
	
	bindedCvars = ArrayCreate(_BINDED_CVAR)
	hookedCvars = ArrayCreate(_HOOCKED_CVAR)

	set_task(0.1, "@amxx_182_compact_refresh_cvars_handlers", generate_task_id(), .flags = "b")
}

@amxx_182_compact_refresh_cvars_handlers() {
	for(new i, record[_HOOCKED_CVAR], value[512], ret; i < ArraySize(hookedCvars); i++) {
		ArrayGetArray(hookedCvars, i, record)
		get_pcvar_string(record[_HC_POINTER], value, charsmax(value))

		if(!equal(value, record[_HC_PREVIOUS_VALUE])) {
			ExecuteForward(record[_HC_CHANGE_CALLBACK], ret, record[_HC_POINTER])
			copy(record[_HC_PREVIOUS_VALUE], charsmax(record[_HC_PREVIOUS_VALUE]), value)
			ArraySetArray(hookedCvars, i, record)
		}
	}

	for(new i, record[_BINDED_CVAR], value, Float:valueFl, valueStr[512]; i < ArraySize(bindedCvars); i++) {
		ArrayGetArray(bindedCvars, i, record)
		
		if(record[_BC_VARIABLE_SIZE] > 1) {
			get_pcvar_string(record[_BC_POINTER], valueStr, charsmax(valueStr))

			for(new i; i < record[_BC_VARIABLE_SIZE]; i++) {
				set_addr_val(record[_BC_VARIABLE_ADDRESS] + (i * 4), valueStr[i])
			}
		}
		else if(record[_BC_VARIABLE_IS_FLOAT]) {
			valueFl = get_pcvar_float(record[_BC_POINTER])
			set_addr_val(record[_BC_VARIABLE_ADDRESS], any:valueFl)
		}
		else {
			value = get_pcvar_num(record[_BC_POINTER])
			set_addr_val(record[_BC_VARIABLE_ADDRESS], value)
		}
	}
}

hook_cvar_change(const pcvar, const callback[]) {
	new record[_HOOCKED_CVAR];
	record[_HC_POINTER] = pcvar;
	record[_HC_CHANGE_CALLBACK] = CreateOneForward(g_pluginId, callback, FP_CELL);
	get_pcvar_string(pcvar, record[_HC_PREVIOUS_VALUE], charsmax(record[_HC_PREVIOUS_VALUE]));

	ArrayPushArray(hookedCvars, record);
}

bind_pcvar_string(const pcvar, dest[], const destSize) {
	get_pcvar_string(pcvar, dest, destSize)

	new record[_BINDED_CVAR];
	record[_BC_POINTER] = pcvar;
	record[_BC_VARIABLE_ADDRESS] = get_var_addr(dest);
	record[_BC_VARIABLE_SIZE] = destSize;

	ArrayPushArray(bindedCvars, record);
}

bind_pcvar_num(const pcvar, &dest) {
	dest = get_pcvar_num(pcvar);

	new record[_BINDED_CVAR];
	record[_BC_POINTER] = pcvar;
	record[_BC_VARIABLE_ADDRESS] = get_var_addr(dest);
	record[_BC_VARIABLE_SIZE] = 1;

	ArrayPushArray(bindedCvars, record);
}

stock bind_pcvar_float(const pcvar, &Float:dest) {
	dest = get_pcvar_float(pcvar);

	new record[_BINDED_CVAR];
	record[_BC_POINTER] = pcvar;
	record[_BC_VARIABLE_ADDRESS] = get_var_addr(dest);
	record[_BC_VARIABLE_SIZE] = 1;
	record[_BC_VARIABLE_IS_FLOAT] = true;

	ArrayPushArray(bindedCvars, record);
}

ArrayFindString(Array:which, const item[]) {
	for(new i, buffer[512]; i < ArraySize(which); i++) {
		ArrayGetString(which, i, buffer, charsmax(buffer))
		if(equal(buffer, item))
			return i;
	}
	return -1;
}

ArrayFindValue(Array:which, any:item) {
	for(new i; i < ArraySize(which); i++) {
		if(item == ArrayGetCell(which, i))
			return i;
	}
	return -1
}