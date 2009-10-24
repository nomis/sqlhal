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

int iter_list_item(void *data, word_t ref, const char *word) {
	FILE *fd = data;
	int ret;
	(void)ref;

	if (word == NULL || word[0] == 0) return -EFAULT;

	ret = fprintf(fd, "%s\n", word);
	if (ret <= 0) return -EIO;

	return OK;
}

int save_list(const char *name, enum list type, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;

	if (name == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	switch (type) {
	case LIST_AUX:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which can be used as keywords only\n"\
			"#\tin order to supplement other keywords\n"\
			"#\n");
		break;
	case LIST_BAN:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which cannot be used as keywords\n"\
			"#\n");
		break;
	case LIST_GREET:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which will be used as keywords whenever\n"\
			"#\tthe judge changes, in order to greet them.\n"\
			"#\n");
		break;
	default:
		ret = -EFAULT;
		goto fail;
	}

	if (ret <= 0) {
		ret = -EIO;
		goto fail;
	}

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_list_iter(brain, type, iter_list_item, fd);
	if (ret) goto fail;

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

int iter_map_item(void *data, word_t key_ref, word_t value_ref, const char *key, const char *value) {
	FILE *fd = data;
	int ret;
	(void)key_ref;
	(void)value_ref;

	if (key == NULL || key[0] == 0) return -EFAULT;
	if (value == NULL || value[0] == 0) return -EFAULT;

	ret = fprintf(fd, "%s\t%s\n", key, value);
	if (ret <= 0) return -EIO;

	return OK;
}

int save_map(const char *name, enum map type, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;

	if (name == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	switch (type) {
	case MAP_SWAP:
		ret = fprintf(fd, "#\n"\
			"#\tThe word on the left is changed to the word on the\n"\
			"#\tright when used as a keyword\n"\
			"#\n");
		break;
		break;
	default:
		ret = -EFAULT;
		goto fail;
	}

	if (ret <= 0) {
		ret = -EIO;
		goto fail;
	}

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_map_iter(brain, type, iter_map_item, fd);
	if (ret) goto fail;

fail:
	fclose(fd);
	return ret;
}
