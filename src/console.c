#include <stdio.h>

#include "output.h"

void log_any(const char *type, int code, const char *msg) {
	printf("%s[%d]: %s\n", type, code, msg);
}

void log_fatal(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}

void log_error(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}

void log_warn(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}

void log_notice(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}

void log_info(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}

void log_debug(const char *type, int code, const char *msg) {
	log_any(type, code, msg);
}
