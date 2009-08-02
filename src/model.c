#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "output.h"

#define COOKIE "MegaHALv8"

enum load_mode {
	LOAD_IGNORE,
	LOAD_FORWARD,
	LOAD_BACKWARD,
	LOAD_APPEND
};

int load_tree(FILE *fd, enum load_mode mode, uint32_t dict_size, word_t *dict_words, db_hand **hand, db_tree *tree) {
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

	if (tree != NULL) {
		if (symbol >= dict_size) {
			log_error("load_tree", symbol, "Symbol references beyond dictionary");
			return -EINVAL;
		}

		tree->word = dict_words[symbol];
		tree->usage = usage;
		tree->count = count;

		ret = db_model_update(hand, tree);
		if (ret) return ret;
	}

	if (branch == 0) return OK;

	switch (mode) {
		case LOAD_FORWARD:
		case LOAD_BACKWARD:
			if (dict_words == NULL || hand == NULL || tree == NULL)
				return -EINVAL;
		case LOAD_IGNORE:
			for (i = 0; i < branch; i++) {
				db_tree *node = NULL;

				if (tree != NULL) {
					ret = db_model_create(hand, &node);
					if (ret) return ret;
				}

				ret = load_tree(fd, mode, dict_size, dict_words, hand, node);
				if (ret) return ret;

				if (tree != NULL) {
					ret = db_model_link(hand, tree, node);
					if (ret) return ret;

					db_model_node_free(&node);
				}
			}
			return OK;
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
	if (*dict_words == NULL) return -ENOMEM;

	for (i = 0; i < size; i++) {
		if (!fread(&length, sizeof(length), 1, fd)) return -EIO;

		tmp[length] = 0;
		if (fread(tmp, sizeof(char), length, fd) != length) return -EIO;

		ret = db_word_use(tmp, &word);
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

int load_brain(char *name, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;
	db_hand *hand;
	char cookie[16];
	number_t order;
	uint8_t tmp8;
	uint32_t dict_size;
	word_t *dict_words;
	db_tree *forward;
	db_tree *backward;

	if (name == NULL || filename == NULL) return -EINVAL;

	log_info("load_brain", 0, filename);

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_model_init(&hand, brain);
	if (ret) goto fail;

	ret = db_model_zap(&hand);
	if (ret) goto fail;

	if (!fread(cookie, sizeof(char), strlen(COOKIE), fd)) return -EIO;
	if (strncmp(cookie, COOKIE, strlen(COOKIE)) != 0) {
		log_error("load_brain", 1, "Not a MegaHAL brain");
		ret = -EIO;
		goto fail;
	}

	fread(&tmp8, sizeof(tmp8), 1, fd);
	order = tmp8;

	ret = db_model_set_order(&hand, order);
	if (ret) goto fail;

	ret = db_model_get_root(&hand, &forward, &backward);
	if (ret) goto fail;

	/* Bah. The word dictionary is at the end of the file.
	 * Either the file can be read twice or we can waste a ton of memory caching the tree.
	 */
	ret = load_tree(fd, LOAD_IGNORE, 0, NULL, NULL, NULL); /* forward */
	if (ret) goto fail;

	log_info("load_brain", 0, "Skipped forward tree");

	ret = load_tree(fd, LOAD_IGNORE, 0, NULL, NULL, NULL); /* backward */
	if (ret) goto fail;

	log_info("load_brain", 0, "Skipped backward tree");

	ret = load_dict(fd, &dict_size, &dict_words);
	if (ret) goto fail;

	log_info("load_brain", dict_size, "Dictionary loaded");

	/* Read most of the file again... */
	rewind(fd);
	fread(&tmp8, sizeof(tmp8), 1, fd);

	ret = load_tree(fd, LOAD_FORWARD, dict_size, dict_words, hand, forward);
	if (ret) goto fail;

	db_model_node_free(&forward);

	log_info("load_brain", 0, "Forward tree loaded");

	ret = load_tree(fd, LOAD_BACKWARD, dict_size, dict_words, hand, backward);
	if (ret) goto fail;

	db_model_node_free(&backward);

	log_info("load_brain", 0, "Backward tree loaded");

	ret = db_model_free(&hand);

	free_dict(&dict_size, &dict_words);

fail:
	fclose(fd);
	return ret;
}
