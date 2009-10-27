#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "dict.h"
#include "err.h"
#include "db.h"
#include "model.h"
#include "output.h"

int input_list(const char *name, const char *prefix, const char *suffix, enum list type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + strlen(suffix) + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.%s", prefix, suffix) <= 0);

	ret = load_list(name, type, filename);
	return ret;
}

int output_list(const char *name, const char *prefix, const char *suffix, enum list type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + strlen(suffix) + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.%s", prefix, suffix) <= 0);

	ret = save_list(name, type, filename);
	return ret;
}

int input_map(const char *name, const char *prefix, const char *suffix, enum map type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + strlen(suffix) + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.%s", prefix, suffix) <= 0);

	ret = load_map(name, type, filename);
	return ret;
}

int output_map(const char *name, const char *prefix, const char *suffix, enum map type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + strlen(suffix) + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.%s", prefix, suffix) <= 0);

	ret = save_map(name, type, filename);
	return ret;
}

int input_brain(const char *name, const char *prefix) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + 3 + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.brn", prefix) <= 0);

	ret = load_brain(name, filename);
	return ret;
}

int output_brain(const char *name, const char *prefix) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 1 + 3 + 1) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	BUG_IF(sprintf(filename, "%s.brn", prefix) <= 0);

	ret = save_brain(name, filename);
	return ret;
}

int main(int argc, char *argv[]) {
	char *action;
	char *name;
	char *prefix;
	int ret;
	int fail = 0;
	char *state;

	if (argc != 4 || (strcmp(argv[1], "load") && strcmp(argv[1], "save"))) {
		printf("Brain manipulation\n");
		printf("Usage: %s load <name> <filename prefix>\n", argv[0]);
		printf("       %s save <name> <filename prefix>\n", argv[0]);
		return 1;
	}

	action = argv[1];
	name = argv[2];
	prefix = argv[3];

	state = "db_connect";
	ret = db_connect();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	state = "db_begin";
	ret = db_begin();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	if (!strcmp(action, "load")) {
		state = "input_list aux";
		ret = input_list(name, prefix, "aux", LIST_AUX);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "input_list ban";
		ret = input_list(name, prefix, "ban", LIST_BAN);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "input_list grt";
		ret = input_list(name, prefix, "grt", LIST_GREET);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "input_map swp";
		ret = input_map(name, prefix, "swp", MAP_SWAP);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "input_brain";
		ret = input_brain(name, prefix);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);
	} else if (!strcmp(action, "save")) {
		state = "output_list aux";
		ret = output_list(name, prefix, "aux", LIST_AUX);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "output_list ban";
		ret = output_list(name, prefix, "ban", LIST_BAN);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "output_list grt";
		ret = output_list(name, prefix, "grt", LIST_GREET);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "output_map swp";
		ret = output_map(name, prefix, "swp", MAP_SWAP);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "output_brain";
		ret = output_brain(name, prefix);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);
	} else {
		fail = 1;
	}

	if (fail) {
		state = "db_rollback";
		ret = db_rollback();
		if (ret) goto fail;
		else log_info("brain", ret, state);
	} else {
		state = "db_commit";
		ret = db_commit();
		if (ret) goto fail;
		else log_info("brain", ret, state);
	}

	state = "db_disconnect";
	ret = db_disconnect();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	return 0;

fail:
	log_fatal("brain", ret, state);
	return 1;
}
