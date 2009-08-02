#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "err.h"
#include "types.h"
#include "db.h"
#include "model.h"
#include "output.h"

int do_list(const char *name, const char *prefix, const char *type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	if (sprintf(filename, "%s.%s", prefix, type) <= 0) return -EFAULT;

	ret = load_list(name, type, filename);
	return ret;
}

int do_map(const char *name, const char *prefix, const char *type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	if (sprintf(filename, "%s.%s", prefix, type) <= 0) return -EFAULT;

	ret = load_map(name, type, filename);
	return ret;
}

int do_brain(const char *name, const char *prefix) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	if (filename == NULL) return -ENOMEM;
	if (sprintf(filename, "%s.brn", prefix) <= 0) return -EFAULT;

	ret = load_brain(name, filename);
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
		printf("Brain loader/saver\n");
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
		state = "do_list aux";
		ret = do_list(name, prefix, "aux");
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "do_list ban";
		ret = do_list(name, prefix, "ban");
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "do_list grt";
		ret = do_list(name, prefix, "grt");
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "do_map swp";
		ret = do_map(name, prefix, "swp");
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);

		state = "do_brain";
		ret = do_brain(name, prefix);
		if (ret) { log_warn("brain", ret, state); fail = 1; }
		else log_info("brain", ret, state);
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
