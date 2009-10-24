#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "megahal.h"
#include "output.h"

int input_file(const char *name, const char *filename) {
	int ret = OK;
	brain_t brain;

	if (name == NULL || filename == NULL) return -EINVAL;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = megahal_train(brain, filename);
	if (ret) goto fail;

fail:
	return ret;
}

int main(int argc, char *argv[]) {
	char *name;
	char *file;
	int ret;
	int fail = 0;
	char *state;

	if (argc != 3) {
		printf("Brain training\n");
		printf("Usage: %s <name> <filename>\n", argv[0]);
		return 1;
	}

	name = argv[1];
	file = argv[2];

	state = "db_connect";
	ret = db_connect();
	if (ret) goto fail;
	else log_info("train", ret, state);

	state = "db_begin";
	ret = db_begin();
	if (ret) goto fail;
	else log_info("train", ret, state);

	state = "input_file";
	ret = input_file(name, file);
	if (ret) { log_warn("train", ret, state); fail = 1; }
	else log_info("train", ret, state);

	if (fail) {
		state = "db_rollback";
		ret = db_rollback();
		if (ret) goto fail;
		else log_info("train", ret, state);
	} else {
		state = "db_commit";
		ret = db_commit();
		if (ret) goto fail;
		else log_info("train", ret, state);
	}

	state = "db_disconnect";
	ret = db_disconnect();
	if (ret) goto fail;
	else log_info("train", ret, state);

	return 0;

fail:
	log_fatal("train", ret, state);
	return 1;
}
