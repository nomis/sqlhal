#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"

int initialise_list(const char *personality, const char *type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	char *list;
	int ret = OK;
	db_hand *hand;

	if (personality == NULL || type == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	list = malloc((strlen(personality) + strlen(type) + 2) * sizeof(char));
	sprintf(list, "%s_%s", personality, type);
	ret = db_list_init(list, &hand);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;
		string = strtok(buffer, "\t \n#");

		if ((string != NULL) && (strlen(string) > 0)) {
			word_t word;

			ret = db_word_get(string, &word);
			if (ret == -ENOTFOUND)
				ret = db_word_add(string, &word);
			if (ret) goto fail;

			ret = db_list_contains(&hand, &word);
			if (ret == -ENOTFOUND)
				ret = db_list_add(&hand, &word);
			if (ret) goto fail;
		}
	}

	ret = db_list_free(&hand);

fail:
	fclose(fd);
	return ret;
}

int initialise_map(const char *personality, const char *type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *from;
	char *to;
	char *map;
	int ret = OK;
	db_hand *hand;

	if (personality == NULL || type == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	map = malloc((strlen(personality) + strlen(type) + 2) * sizeof(char));
	sprintf(map, "%s_%s", personality, type);
	ret = db_map_init(map, &hand);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;

		from = strtok(buffer, "\t ");
		to = strtok(NULL, "\t \n#");

		if ((from != NULL) && (strlen(from) > 0) && (to != NULL) && (strlen(to) > 0)) {
			word_t key, value;

			ret = db_word_get(from, &key);
			if (ret == -ENOTFOUND)
				ret = db_word_add(from, &key);
			if (ret) goto fail;

			ret = db_word_get(to, &value);
			if (ret == -ENOTFOUND)
				ret = db_word_add(to, &value);
			if (ret) goto fail;

			ret = db_map_get(&hand, &key, &value);
			if (ret == -ENOTFOUND)
				ret = db_map_add(&hand, &key, &value);
			if (ret) goto fail;
		}
	}

	ret = db_map_free(&hand);

fail:
	fclose(fd);
	return ret;
}
