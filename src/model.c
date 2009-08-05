#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "output.h"

#define COOKIE "MegaHALv8"
#define TOKENS 2
#define TOKEN_ERROR_IDX 0
#define TOKEN_ERROR "<ERROR>"
#define TOKEN_FIN_IDX 1
#define TOKEN_FIN "<FIN>"

enum load_mode {
	LOAD_IGNORE,
	LOAD_STORE
};

int load_tree(FILE *fd, enum load_mode mode, uint_fast32_t dict_size, word_t *dict_words, brain_t brain, db_tree *tree) {
	uint16_t symbol;
	uint32_t usage;
	uint16_t count;
	uint16_t branch;
	int ret;
	uint_fast16_t i;

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

		ret = db_model_update(brain, tree);
		if (ret) return ret;
	}

	if (branch == 0) return OK;

	switch (mode) {
		case LOAD_STORE:
			if (dict_words == NULL || brain == 0 || tree == NULL)
				return -EINVAL;
		case LOAD_IGNORE:
			for (i = 0; i < branch; i++) {
				db_tree *node = NULL;

				if (mode == LOAD_STORE) {
					node = db_model_node_alloc();
					if (node == NULL) return -ENOMEM;

					ret = db_model_link(tree, node);
					if (ret) return ret;
				}

				ret = load_tree(fd, mode, dict_size, dict_words, brain, node);
				if (ret) return ret;

				if (mode == LOAD_STORE) {
					db_model_node_free(&node);
				}
			}
			return OK;
	}

	return -EFAULT;
}

int load_dict(FILE *fd, uint_fast32_t *dict_size, word_t **dict_words) {
	uint32_t size;
	uint8_t length;
	int ret;
	word_t word;
	char tmp[256];
	uint_fast32_t i;

	if (!fread(&size, sizeof(size), 1, fd)) return -EIO;

	*dict_size = size;
	*dict_words = malloc(sizeof(word_t) * size);
	if (*dict_words == NULL) return -ENOMEM;

	for (i = 0; i < *dict_size; i++) {
		if (!fread(&length, sizeof(length), 1, fd)) return -EIO;

		tmp[length] = 0;
		if (fread(tmp, sizeof(char), length, fd) != length) return -EIO;

		switch (i) {
		case TOKEN_ERROR_IDX:
			if (strcmp(tmp, TOKEN_ERROR)) {
				log_error("load_dict", i, "Invalid word (not " TOKEN_ERROR ")");
				return -EINVAL;
			}
			(*dict_words)[i] = 0;
			break;

		case TOKEN_FIN_IDX:
			if (strcmp(tmp, TOKEN_FIN)) {
				log_error("load_dict", i, "Invalid word (not " TOKEN_FIN ")");
				return -EINVAL;
			}
			(*dict_words)[i] = 0;
			break;

		default:
			ret = db_word_use(tmp, &word);
			if (ret) return ret;

			(*dict_words)[i] = word;
		}
	}

	return OK;
}

void free_loaded_dict(uint_fast32_t *dict_size, word_t **dict_words) {
	free(*dict_words);

	*dict_size = 0;
	*dict_words = NULL;
}

void free_saved_dict(uint_fast32_t *dict_size, word_t **dict_words, char ***dict_text) {
	uint_fast32_t i;

	for (i = TOKENS; i < *dict_size; i++)
		free((*dict_text)[i]);

	free(*dict_words);
	free(*dict_text);

	*dict_size = 0;
	*dict_words = NULL;
	*dict_text = NULL;
}

int read_dict(brain_t brain, uint_fast32_t *dict_size, word_t **dict_words, char ***dict_text) {
	int ret;

	*dict_size = 0;

	*dict_words = malloc(sizeof(word_t) * TOKENS);
	if (*dict_words == NULL) return -ENOMEM;

	*dict_text = malloc(sizeof(char *) * TOKENS);
	if (*dict_text == NULL) return -ENOMEM;

	(*dict_words)[TOKEN_ERROR_IDX] = 0;
	(*dict_text)[TOKEN_ERROR_IDX] = TOKEN_ERROR;

	(*dict_words)[TOKEN_FIN_IDX] = 0;
	(*dict_text)[TOKEN_FIN_IDX] = TOKEN_FIN;

	*dict_size = TOKENS;

	ret = db_model_dump_words(brain, dict_size, dict_words, dict_text);
	if (ret) {
		free_saved_dict(dict_size, dict_words, dict_text);
		return ret;
	}

	return OK;
}

int save_dict(FILE *fd, uint_fast32_t dict_size, char **dict_text) {
	uint8_t length;
	uint32_t _dict_size;
	uint_fast32_t i;

	if (!fwrite(&_dict_size, sizeof(_dict_size), 1, fd)) return -EIO;

	for (i = 0; i < dict_size; i++) {
		length = strlen(dict_text[i]);
		if (!fwrite(&length, sizeof(length), 1, fd)) return -EIO;
		if (fwrite(dict_text[i], sizeof(char), length, fd) != length) return -EIO;
	}

	return OK;
}

int load_brain(char *name, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;
	char cookie[16];
	number_t order;
	uint8_t tmp8;
	uint_fast32_t dict_size;
	word_t *dict_words;
	db_tree *forward;
	db_tree *backward;

	if (name == NULL || filename == NULL) return -EINVAL;

	log_info("load_brain", 0, filename);

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_model_zap(brain);
	if (ret) goto fail;

	if (fread(cookie, sizeof(char), strlen(COOKIE), fd) != strlen(COOKIE)) return -EIO;
	if (strncmp(cookie, COOKIE, strlen(COOKIE)) != 0) {
		log_error("load_brain", 1, "Not a MegaHAL brain");
		ret = -EIO;
		goto fail;
	}

	if (!fread(&tmp8, sizeof(tmp8), 1, fd)) return -EIO;
	order = tmp8;

	ret = db_model_set_order(brain, order);
	if (ret) goto fail;

	ret = db_model_get_root(brain, &forward, &backward);
	if (ret) goto fail;

	/* Bah. The word dictionary is at the end of the file.
	 * Either the file can be read twice or we can waste a ton of memory caching the tree.
	 */
	ret = load_tree(fd, LOAD_IGNORE, 0, NULL, 0, NULL); /* forward */
	if (ret) goto fail;

	log_info("load_brain", 0, "Skipped forward tree");

	ret = load_tree(fd, LOAD_IGNORE, 0, NULL, 0, NULL); /* backward */
	if (ret) goto fail;

	log_info("load_brain", 0, "Skipped backward tree");

	ret = load_dict(fd, &dict_size, &dict_words);
	if (ret) goto fail;

	log_info("load_brain", dict_size, "Dictionary loaded");

	/* Read most of the file again... */
	if (fseek(fd, sizeof(char) * strlen(COOKIE) + sizeof(tmp8), SEEK_SET)) return -EIO;

	ret = load_tree(fd, LOAD_STORE, dict_size, dict_words, brain, forward);
	if (ret) goto fail;

	db_model_node_free(&forward);

	log_info("load_brain", 0, "Forward tree loaded");

	ret = load_tree(fd, LOAD_STORE, dict_size, dict_words, brain, backward);
	if (ret) goto fail;

	db_model_node_free(&backward);

	log_info("load_brain", 0, "Backward tree loaded");

	free_loaded_dict(&dict_size, &dict_words);

fail:
	fclose(fd);
	return ret;
}

int save_brain(char *name, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;
	number_t order;
	uint_fast32_t dict_size;
	word_t *dict_words;
	char **dict_text;
	db_tree *forward;
	db_tree *backward;

	if (name == NULL || filename == NULL) return -EINVAL;

	log_info("save_brain", 0, filename);

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_model_get_order(brain, &order);
	if (ret) goto fail;

	ret = db_model_get_root(brain, &forward, &backward);
	if (ret) goto fail;

	ret = read_dict(brain, &dict_size, &dict_words, &dict_text);
	if (ret) goto fail;

	log_info("save_brain", dict_size, "Dictionary read");

	if (fwrite(COOKIE, sizeof(char), strlen(COOKIE), fd) != strlen(COOKIE)) return -EIO;
	if (!fwrite(&order, sizeof(order), 1, fd)) return -EIO;
#if 0
	ret = save_tree(fd, dict_size, dict_words, brain, forward); /* forward */
	if (ret) goto fail;

	log_info("save_brain", 0, "Forward tree saved");

	ret = save_tree(fd, dict_size, dict_words, brain, backward); /* backward */
	if (ret) goto fail;

	log_info("save_brain", 0, "Backward tree saved");
#endif
	ret = save_dict(fd, dict_size, dict_text);
	if (ret) goto fail;

	log_info("save_brain", dict_size, "Dictionary saved");

	free_saved_dict(&dict_size, &dict_words, &dict_text);

fail:
	fclose(fd);
	return ret;
}
