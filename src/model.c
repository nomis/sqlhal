#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "model.h"
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

	WARN_IF(fd == NULL);

	if (!fread(&symbol, sizeof(symbol), 1, fd)) return -EIO;
	if (!fread(&usage, sizeof(usage), 1, fd)) return -EIO;
	if (!fread(&count, sizeof(count), 1, fd)) return -EIO;
	if (!fread(&branch, sizeof(branch), 1, fd)) return -EIO;

	if (mode == LOAD_STORE) {
		WARN_IF(dict_words == NULL);
		WARN_IF(brain == 0);
		WARN_IF(tree == NULL);

		if (symbol >= dict_size) {
			log_error("load_tree", symbol, "Symbol references beyond end of dictionary");
			WARN();
		}

		tree->word = dict_words[symbol];
		tree->usage = usage;
		tree->count = count;

		ret = db_model_update(brain, tree);
		if (ret) return ret;
	}

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
				WARN();
			}
			(*dict_words)[i] = 0;
			break;

		case TOKEN_FIN_IDX:
			if (strcmp(tmp, TOKEN_FIN)) {
				log_error("load_dict", i, "Invalid word (not " TOKEN_FIN ")");
				WARN();
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

void free_saved_dict(uint_fast32_t *dict_size, word_t **dict_words, uint32_t **dict_idx, char ***dict_text) {
	uint_fast32_t i;

	for (i = TOKENS; i < *dict_size; i++)
		free((*dict_text)[i]);

	free(*dict_words);
	free(*dict_idx);
	free(*dict_text);

	*dict_size = 0;
	*dict_words = NULL;
	*dict_idx = NULL;
	*dict_text = NULL;
}

int read_dict(brain_t brain, uint_fast32_t *dict_size, word_t **dict_words, uint32_t **dict_idx, char ***dict_text) {
	int ret;

	*dict_size = 0;

	*dict_words = malloc(sizeof(word_t) * TOKENS);
	if (*dict_words == NULL) return -ENOMEM;

	*dict_idx = malloc(sizeof(uint32_t) * TOKENS);
	if (*dict_idx == NULL) return -ENOMEM;

	*dict_text = malloc(sizeof(char *) * TOKENS);
	if (*dict_text == NULL) return -ENOMEM;

	(*dict_words)[TOKEN_ERROR_IDX] = 0;
	(*dict_idx)[TOKEN_ERROR_IDX] = TOKEN_ERROR_IDX;
	(*dict_text)[TOKEN_ERROR_IDX] = TOKEN_ERROR;

	(*dict_words)[TOKEN_FIN_IDX] = 0;
	(*dict_idx)[TOKEN_FIN_IDX] = TOKEN_FIN_IDX;
	(*dict_text)[TOKEN_FIN_IDX] = TOKEN_FIN;

	*dict_size = TOKENS;

	ret = db_model_dump_words(brain, dict_size, dict_words, dict_idx, dict_text);
	if (ret) {
		free_saved_dict(dict_size, dict_words, dict_idx, dict_text);
		return ret;
	}

	return OK;
}

int save_dict(FILE *fd, uint_fast32_t dict_size, char **dict_text) {
	uint8_t length;
	uint32_t _dict_size;
	uint_fast32_t i;

	_dict_size = dict_size;
	if (!fwrite(&_dict_size, sizeof(_dict_size), 1, fd)) return -EIO;

	for (i = 0; i < dict_size; i++) {
		length = strlen(dict_text[i]);
		if (!fwrite(&length, sizeof(length), 1, fd)) return -EIO;
		if (fwrite(dict_text[i], sizeof(char), length, fd) != length) return -EIO;
	}

	return OK;
}

int find_word(uint_fast32_t dict_size, word_t *dict_words, uint32_t *dict_idx, word_t word, uint16_t *symbol) {
	uint_fast32_t min = 0;
	uint_fast32_t pos;
	uint_fast32_t max = dict_size - 1;

	WARN_IF(word == 0);

	while (1) {
		pos = (min + max) / 2;

		if (word == dict_words[pos]) {
			BUG_IF(pos == TOKEN_ERROR_IDX);
			BUG_IF(pos == TOKEN_FIN_IDX);
			*symbol = dict_idx[pos];
			return OK;
		} else if (word > dict_words[pos]) {
			if (max == pos) {
				log_error("find_word", word, "Word missing from dictionary");
				return -ENOTFOUND;
			}
			min = pos + 1;
		} else {
			if (min == pos) {
				log_error("find_word", word, "Word missing from dictionary");
				return -ENOTFOUND;
			}
			max = pos - 1;
		}
	}
}

int save_tree(FILE *fd, uint_fast32_t dict_size, word_t *dict_words, uint32_t *dict_idx, brain_t brain, db_tree **tree) {
	db_tree *tree_p;
	uint16_t symbol;
	uint32_t usage;
	uint16_t count;
	uint16_t branch;
	int ret;
	uint_fast32_t i;

	WARN_IF(fd == NULL);
	WARN_IF(dict_words == NULL);
	WARN_IF(brain == 0);
	WARN_IF(tree == NULL);
	WARN_IF(*tree == NULL);

	if (dict_size > UINT16_MAX)
		return -ENOSPC;

	tree_p = *tree;

	ret = db_model_node_fill(brain, (db_tree *)tree_p->nodes[i]);
	if (ret) return ret;

	if (tree_p->word == 0) {
		if (tree_p->parent_id == 0) {
			symbol = TOKEN_ERROR_IDX;
		} else {
			BUG_IF(tree_p->usage != 0);
			BUG_IF(tree_p->children != 0);

			symbol = TOKEN_FIN_IDX;
		}
	} else {
		ret = find_word(dict_size, dict_words, dict_idx, tree_p->word, &symbol);
		if (ret) return ret;
	}

	usage = tree_p->usage > UINT32_MAX ? UINT32_MAX : tree_p->usage;
	count = tree_p->count > UINT16_MAX ? UINT16_MAX : tree_p->count;

	if (tree_p->children > UINT16_MAX)
		return -ENOSPC;
	branch = tree_p->children;

	if (!fwrite(&symbol, sizeof(symbol), 1, fd)) return -EIO;
	if (!fwrite(&usage, sizeof(usage), 1, fd)) return -EIO;
	if (!fwrite(&count, sizeof(count), 1, fd)) return -EIO;
	if (!fwrite(&branch, sizeof(branch), 1, fd)) return -EIO;

	for (i = 0; i < tree_p->children; i++) {
		ret = save_tree(fd, dict_size, dict_words, dict_idx, brain, (db_tree **)&tree_p->nodes[i]);
		if (ret) return ret;
	}
	db_model_node_free(tree);

	return OK;
}

int load_brain(const char *name, const char *filename) {
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

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

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

int save_brain(const char *name, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;
	number_t order;
	uint8_t tmp8;
	uint_fast32_t dict_size;
	word_t *dict_words;
	uint32_t *dict_idx;
	char **dict_text;
	db_tree *forward;
	db_tree *backward;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	log_info("save_brain", 0, filename);

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_model_get_order(brain, &order);
	if (ret) goto fail;

	if (order > UINT8_MAX) return -ENOSPC;
	tmp8 = order;

	ret = db_model_get_root(brain, &forward, &backward);
	if (ret) goto fail;

	ret = read_dict(brain, &dict_size, &dict_words, &dict_idx, &dict_text);
	if (ret) goto fail;

	log_info("save_brain", dict_size, "Dictionary read");

	if (fwrite(COOKIE, sizeof(char), strlen(COOKIE), fd) != strlen(COOKIE)) return -EIO;
	if (!fwrite(&tmp8, sizeof(tmp8), 1, fd)) return -EIO;

	ret = save_tree(fd, dict_size, dict_words, dict_idx, brain, &forward); /* forward */
	if (ret) goto fail;

	log_info("save_brain", 0, "Forward tree saved");

	ret = save_tree(fd, dict_size, dict_words, dict_idx, brain, &backward); /* backward */
	if (ret) goto fail;

	log_info("save_brain", 0, "Backward tree saved");

	ret = save_dict(fd, dict_size, dict_text);
	if (ret) goto fail;

	log_info("save_brain", dict_size, "Dictionary saved");

	free_saved_dict(&dict_size, &dict_words, &dict_idx, &dict_text);

fail:
	fclose(fd);
	return ret;
}

int model_alloc(brain_t brain, model_t **model) {
	model_t *model_p;
	uint_fast32_t i;
	int ret;

	*model = malloc(sizeof(model_t));
	if (*model == NULL) return -ENOMEM;
	model_p = *model;

	model_p->brain = brain;

	ret = db_model_get_order(brain, &model_p->order);
	if (ret) goto fail;

	model_p->contexts = malloc(sizeof(db_tree *) * model_p->order);
	if (model_p->contexts == NULL) { ret = -ENOMEM; goto fail; }
	for (i = 0; i < model_p->order; i++)
		model_p->contexts[i] = NULL;

	return OK;

fail:
	if (model_p->contexts != NULL)
		free(model_p->contexts);

	free(*model);
	*model = NULL;
	return ret;
}

void model_free(model_t **model) {
	model_t *model_p;
	uint_fast32_t i;

	if (*model == NULL) return;
	model_p = *model;

	for (i = 0; i < model_p->order; i++)
		if (model_p->contexts[i] != NULL
				&& model_p->contexts[i] != model_p->forward
				&& model_p->contexts[i] != model_p->backward)
			db_model_node_free(&model_p->contexts[i]);

	db_model_node_free(&model_p->forward);
	db_model_node_free(&model_p->backward);

	free(*model);
	*model = NULL;
}
