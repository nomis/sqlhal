#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "dbtree.h"
#include "output.h"

#define COOKIE "MegaHALv8"
#define LOAD_IGNORE 0
#define LOAD_FORWARD 1
#define LOAD_BACKWARD 2
#define LOAD_APPEND 3

int load_tree(FILE *fd, int mode, word_t *dict, db_hand **hand, db_tree **tree) {
	uint16_t symbol;
	uint32_t usage;
	uint16_t count;
	uint16_t branch;
	int ret;
	uint16_t i;

	if (!fread(&symbol, sizeof(symbol), 1, fd)) return -EIO;
	if (!fread(&usage, sizeof(usage), 1, fd)) return -EIO;
	if (!fread(&count, sizeof(count), 1, fd)) return -EIO;
	if (!fread(&branch, sizeof(branch), 1, fd)) return -EIO;

	if (branch == 0) return OK;

	switch (mode) {
		case LOAD_IGNORE:
			for (i = 0; i < branch; i++) {
				ret = load_tree(fd, mode, dict, hand, tree);
				if (ret) return ret;
			}
			return OK;
		case LOAD_FORWARD:
		case LOAD_BACKWARD:
		case LOAD_APPEND:
			break;
	}

	return -EFAULT;
}

int load_dict(FILE *fd, uint32_t *dict_size, word_t **dict_words) {
	uint32_t size;
	uint8_t length;
	int ret;
	word_t word;
	char tmp[256];
	uint32_t i;

	if (!fread(&size, sizeof(size), 1, fd)) return -EIO;

	*dict_size = size;
	*dict_words = malloc(sizeof(word_t) * size);

	for (i = 0; i < size; i++) {
		if (!fread(&length, sizeof(length), 1, fd)) return -EIO;

		tmp[length] = 0;
		if (fread(tmp, sizeof(char), length, fd) != length) return -EIO;

		ret = db_word_get(tmp, &word);
		if (ret == -ENOTFOUND)
			ret = db_word_add(tmp, &word);
		if (ret) return ret;

		(*dict_words)[i] = word;
	}

	return OK;
}

void free_dict(uint32_t *dict_size, word_t **dict_words) {
	free(*dict_words);

	*dict_size = 0;
	*dict_words = NULL;
}

int load_brain(const char *name, const char *filename) {
	FILE *fd;
	int ret = OK;
#if 0
	db_hand *hand;
#endif
	char cookie[16];
	number_t order;
	uint8_t tmp8;
	uint32_t dict_size;
	word_t *dict_words;

	if (name == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

#if 0
	ret = db_model_zap(name, &hand);
	if (ret && ret != -ENOTFOUND) goto fail;

	ret = db_model_init(name, &hand);
	if (ret) goto fail;
#endif

	if (!fread(cookie, sizeof(char), strlen(COOKIE), fd)) return -EIO;
	if (strncmp(cookie, COOKIE, strlen(COOKIE)) != 0) {
		log_error("load_brain", 1, "Not a MegaHAL brain");
		ret = -EIO;
		goto fail;
	}

	fread(&tmp8, sizeof(tmp8), 1, fd);
	order = tmp8;
#if 0
	ret = db_model_set_order(&hand, order);
	if (ret) goto fail;
#endif

	/* Bah. The word dictionary is at the end of the file.
	 * Either the file can be read twice or we can waste a ton of memory caching the tree.
	 */
	ret = load_tree(fd, LOAD_IGNORE, NULL, NULL, NULL); /* forward */
	if (ret) goto fail;

	ret = load_tree(fd, LOAD_IGNORE, NULL, NULL, NULL); /* backward */
	if (ret) goto fail;

	ret = load_dict(fd, &dict_size, &dict_words);
	if (ret) goto fail;

	/* Read most of the file again... */
	rewind(fd);
	fread(&tmp8, sizeof(tmp8), 1, fd);

#if 0
	ret = load_tree(fd, LOAD_FORWARD, dict_words, hand, NULL); /* forward */
	if (ret) goto fail;

	ret = load_tree(fd, LOAD_BACKWARD, dict_words, hand, NULL); /* backward */
	if (ret) goto fail;

	ret = db_model_free(&hand);
#endif

	free_dict(&dict_size, &dict_words);

fail:
	fclose(fd);
	return ret;
}
