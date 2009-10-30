#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "megahal.h"

static int learn_text(const char *name, const char *text) {
	int ret = OK;
	brain_t brain;

	if (name == NULL) return -EINVAL;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = megahal_process(brain, text, NULL, MEGAHAL_F_LEARN);
	if (ret) goto fail;

fail:
	return ret;
}

int main(int argc, char *argv[]) {
	char buffer[1024];
	char *name;
	char *text;
	int ret;
	int fail = 0;

	if (argc != 2 && argc != 3) {
		printf("Brain access for learning\n");
		printf("Usage: %s <name> [text]\n", argv[0]);
		printf("  Text will be read from stdin if not specified\n");
		return 1;
	}

	name = argv[1];
	text = argc == 3 ? argv[2] : NULL;

	ret = db_connect();
	if (ret) {
		fprintf(stderr, "<Unable to connect to database (%d)>\n", ret);
		return 1;
	}

	ret = db_begin();
	if (ret) {
		fprintf(stderr, "<Unable to begin database transaction (%d)>\n", ret);
		return 1;
	}

	while (argc == 2 || text != NULL) {
		if (text == NULL) {
			if (fgets(buffer, 1024, stdin) == NULL) {
				break;
			} else {
				text = strtok(buffer, "\r\n");
			}
		}

		if (text != NULL && strlen(text) == 0)
			text = NULL;

		ret = learn_text(name, text);
		if (ret) {
			fprintf(stderr, "<Unable to learn text (%d)>\n", ret);
			fail = 1;
			break;
		}

		text = NULL;
	}

	if (fail) {
		ret = db_rollback();
		if (ret) {
			fprintf(stderr, "<Unable to rollback database transaction (%d)>\n", ret);
			return 1;
		}
	} else {
		ret = db_commit();
		if (ret) {
			fprintf(stderr, "<Unable to commit database transaction (%d)>\n", ret);
			return 1;
		}
	}

	ret = db_disconnect();
	if (ret) {
		fprintf(stderr, "<Unable to disconnect from database (%d)>\n", ret);
		return 1;
	}

	return fail ? 1 : 0;
}
