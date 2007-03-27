#include <stdio.h>

#include "output.h"

void log_error(const char *type, int code, const char *msg) {
	printf("%s[%d]: %s\n", type, code, msg);
}
