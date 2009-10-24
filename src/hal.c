#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "megahal.h"
#include "output.h"

int hal_text(const char *name, const char *text, char **reply) {
	int ret = OK;
	brain_t brain;

	if (name == NULL || text == NULL) return -EINVAL;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = megahal_process(brain, text, reply, MEGAHAL_LEARN|MEGAHAL_REPLY);
	if (ret) goto fail;

fail:
	return ret;
}

int main(int argc, char *argv[]) {
	char buffer[1024];
	char *name;
	char *text;
	char *reply;
	int ret;
	int fail = 0;
	char *state;

	if (argc != 2 && argc != 3) {
		printf("Brain access for learning and responses\n");
		printf("Usage: %s <name> [text]\n", argv[0]);
		printf("  Text will be read from stdin if not specified\n");
		return 1;
	}

	name = argv[1];
	text = argc == 3 ? argv[2] : NULL;

	state = "db_connect";
	ret = db_connect();
	if (ret) goto fail;
	else log_info("hal", ret, state);

	state = "db_begin";
	ret = db_begin();
	if (ret) goto fail;
	else log_info("hal", ret, state);

	if (text == NULL) {
		state = "read_text";
		if (fgets(buffer, 1024, stdin) == NULL) {
			log_fatal("hal", -EIO, state);
			fail = 1;
		} else {
			text = strtok(buffer, "\r\n");
		}
	}

	state = "hal_text";
	if (!fail && (text == NULL || strlen(text) == 0)) {
		log_fatal("hal", -EINVAL, state);
		fail = 1;
	} else {
		reply = NULL;
		ret = hal_text(name, text, &reply);
		if (ret) { log_warn("hal", ret, state); fail = 1; }
		else if (reply == NULL || strlen(reply) == 0) {
			log_warn("hal", ret, state);
			fail = 1;
		}
		else log_info("megahal-reply", ret, reply);
	}

	if (fail) {
		state = "db_rollback";
		ret = db_rollback();
		if (ret) goto fail;
		else log_info("hal", ret, state);
	} else {
		state = "db_commit";
		ret = db_commit();
		if (ret) goto fail;
		else log_info("hal", ret, state);
	}

	state = "db_disconnect";
	ret = db_disconnect();
	if (ret) goto fail;
	else log_info("hal", ret, state);

	return 0;

fail:
	log_fatal("hal", ret, state);
	return 1;
}
