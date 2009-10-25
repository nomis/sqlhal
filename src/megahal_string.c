#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"

void megahal_capitalise(char *string) {
	size_t i, len;
	int start = 1;

	if (string == NULL) return;
	len = strlen(string);

	for (i = 0; i < len; i++) {
		if (isalpha((unsigned char)string[i])) {
			if (start) string[i] = (unsigned char)toupper((unsigned char)string[i]);
			else string[i] = (unsigned char)tolower((unsigned)string[i]);
			start = 0;
		}
		if ((i > 2) && (strchr("!.?", (unsigned char)string[i-1]) != NULL) && (isspace((unsigned char)string[i])))
			start = 1;
	}
}

void megahal_upper(char *string) {
	size_t i, len;

	if (string == NULL) return;
	len = strlen(string);

	for (i = 0; i < len; i++)
		string[i] = (unsigned char)toupper((unsigned char)string[i]);
}

int megahal_parse(const char *string, list_t **words) {
	(void)string;
	(void)words;

	return -EFAULT;
}
