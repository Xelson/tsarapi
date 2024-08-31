#include <amxmodx>
#include <easy_http>
#include <tsarapi_util>

enum ConfigOptionType {
	config_option_number,
	config_option_string,
	config_option_real
}

enum _:CONFIG_OPTIONS {
	CO_NAME[32],
	CO_VALUE[128],
	ConfigOptionType:CO_TYPE,
	CO_CVAR_ID
}

static Trie:trieOptionsByCvarId;
static Array:arrOptionsByCvarIdIndex;

config_observer_init() {
	trieOptionsByCvarId = TrieCreate();
	arrOptionsByCvarIdIndex = ArrayCreate();

	set_task(10.0, "@config_observer_task_queue_event", generate_task_id())
}

config_observer_watch_cvar(cvarId, const optionName[], ConfigOptionType:optionType) {
	new data[CONFIG_OPTIONS];
	copy(data[CO_NAME], charsmax(data[CO_NAME]), optionName);
	get_pcvar_string(cvarId, data[CO_VALUE], charsmax(data[CO_VALUE]));
	data[CO_TYPE] = optionType;
	data[CO_CVAR_ID] = cvarId;

	TrieSetArray(trieOptionsByCvarId, _fmt("%d", cvarId), data, sizeof(data));
	if(ArrayFindValue(arrOptionsByCvarIdIndex, cvarId) == -1)
		ArrayPushCell(arrOptionsByCvarIdIndex, cvarId)

	hook_cvar_change(cvarId, "@config_observer_on_cvar_change");
}

@config_observer_on_cvar_change(cvarId) {
	new data[CONFIG_OPTIONS];

	if(TrieGetArray(trieOptionsByCvarId, _fmt("%d", cvarId), data, sizeof(data))) {
		get_pcvar_string(cvarId, data[CO_VALUE], charsmax(data[CO_VALUE]));
		TrieSetArray(trieOptionsByCvarId, _fmt("%d", cvarId), data, sizeof(data));

		config_observer_queue_event();
	}
} 

@config_observer_task_queue_event() {
	config_observer_queue_event()
}

config_observer_queue_event() {
	new EzJSON:root = ezjson_init_object();

	for(new i, cvarId, data[CONFIG_OPTIONS]; i < ArraySize(arrOptionsByCvarIdIndex); i++) {
		cvarId = ArrayGetCell(arrOptionsByCvarIdIndex, i)

		if(TrieGetArray(trieOptionsByCvarId, _fmt("%d", cvarId), data, sizeof(data))) {
			switch(data[CO_TYPE]) {
				case config_option_number: ezjson_object_set_number(root, data[CO_NAME], str_to_num(data[CO_VALUE]));
				case config_option_real: ezjson_object_set_real(root, data[CO_NAME], str_to_float(data[CO_VALUE]));
				case config_option_string: ezjson_object_set_string(root, data[CO_NAME], data[CO_VALUE]);
			}
		}
	}

	queue_event_emit(
		"config", root,
		.replaceIfHandler = "@config_observer_replace_event_if"
	);
}

@config_observer_replace_event_if() {
	return 0; // we return the first pos because we need to send only fresh data from the queue
}