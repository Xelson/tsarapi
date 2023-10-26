#if defined _ezjson_gc_utils_included
	#endinput
#endif
#define _ezjson_gc_utils_included

#include <easy_http>
#include <cellarray>

stock EzJSON_GC:ezjson_gc_init() {
	return EzJSON_GC:ArrayCreate();
}

stock ezjson_gc_destroy(EzJSON_GC:gc) {
	new Array:handles = Array:gc;
	for(new i, EzJSON:handle; i < ArraySize(handles); i++) {
		handle = EzJSON:ArrayGetCell(handles, i);
		ezjson_free(handle);
	}
	ArrayDestroy(handles);
}

stock ezjson_gc_push(EzJSON_GC:gc, EzJSON:handle) {
	ArrayPushCell(Array:gc, handle);
}

stock operator+(EzJSON_GC:gc, EzJSON:handle) {
	ezjson_gc_push(gc, handle);
	return 0;
}