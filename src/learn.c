#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "megahal.h"
#include "output.h"

int learn_text(const char *name, const char *text) {
	int ret = OK;
	brain_t brain;

	if (name == NULL || text == NULL) return -EINVAL;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = megahal_process(brain, text, NULL, MEGAHAL_LEARN);
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
	char *state;

	if (argc != 2 && argc != 3) {
		printf("Brain access for learning\n");
		printf("Usage: %s <name> [text]\n", argv[0]);
		printf("  Text will be read from stdin if not specified\n");
		return 1;
	}

	name = argv[1];
	text = argc == 3 ? argv[2] : NULL;

	state = "db_connect";
	ret = db_connect();
	if (ret) goto fail;
	else log_info("learn", ret, state);

	state = "db_begin";
	ret = db_begin();
	if (ret) goto fail;
	else log_info("learn", ret, state);

	if (text == NULL) {
		state = "read_text";
		if (fgets(buffer, 1024, stdin) == NULL) {
			log_fatal("learn", -EIO, state);
			fail = 1;
		} else {
			text = strtok(buffer, "\r\n");
		}
	}

	state = "learn_text";
	if (!fail && (text == NULL || strlen(text) == 0)) {
		log_fatal("learn", -EINVAL, state);
		fail = 1;
	} else {
		ret = learn_text(name, text);
		if (ret) { log_warn("learn", ret, state); fail = 1; }
		else log_info("learn", ret, state);
	}

	if (fail) {
		state = "db_rollback";
		ret = db_rollback();
		if (ret) goto fail;
		else log_info("learn", ret, state);
	} else {
		state = "db_commit";
		ret = db_commit();
		if (ret) goto fail;
		else log_info("learn", ret, state);
	}

	state = "db_disconnect";
	ret = db_disconnect();
	if (ret) goto fail;
	else log_info("learn", ret, state);

	return 0;

fail:
	log_fatal("learn", ret, state);
	return 1;
}
