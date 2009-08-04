#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"

int load_list(const char *name, enum list type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	int ret = OK;
	brain_t brain;

	if (name == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_list_zap(brain, type);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;
		string = strtok(buffer, "\t \n#");

		if ((string != NULL) && (strlen(string) > 0)) {
			word_t word;

			ret = db_word_use(string, &word);
			if (ret) goto fail;

			ret = db_list_contains(brain, type, word);
			if (ret == -ENOTFOUND)
				ret = db_list_add(brain, type, word);
			if (ret) goto fail;
		}
	}

fail:
	fclose(fd);
	return ret;
}

int load_map(const char *name, enum map type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *from;
	char *to;
	int ret = OK;
	brain_t brain;

	if (name == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_map_zap(brain, type);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;

		from = strtok(buffer, "\t ");
		to = strtok(NULL, "\t \n#");

		if ((from != NULL) && (strlen(from) > 0) && (to != NULL) && (strlen(to) > 0)) {
			word_t key, value;

			ret = db_word_use(from, &key);
			if (ret) goto fail;

			ret = db_word_use(to, &value);
			if (ret) goto fail;

			ret = db_map_get(brain, type, key, &value);
			if (ret == -ENOTFOUND)
				ret = db_map_put(brain, type, key, value);
			if (ret) goto fail;
		}
	}

fail:
	fclose(fd);
	return ret;
}
