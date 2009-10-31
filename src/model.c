#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "model.h"
#include "output.h"

#define COOKIE_M8 "MegaHALv8"
#define COOKIE_S0 "SHAL\x80\x0D\x0A\x1A\x0A"
#define COOKIE_LEN 9

#define TOKENS 2
#define TOKEN_ERROR_IDX 0
#define TOKEN_ERROR "<ERROR>"
#define TOKEN_FIN_IDX 1
#define TOKEN_FIN "<FIN>"

enum load_mode {
	LOAD_IGNORE,
	LOAD_STORE
};

enum size_type {
	SZ_8 = 0,
	SZ_16 = 1,
	SZ_32 = 2,
	SZ_64 = 3
};

typedef struct {
	brain_t brain;
	number_t order;

	FILE *fd;
	enum file_type type;
	enum load_mode mode;

	uint_fast32_t dict_size;
	word_t *dict_words;
} load_t;

typedef struct {
	word_t word;
	uint32_t idx;
} sdict_t;

typedef struct {
	brain_t brain;
	number_t order;

	FILE *fd;
	enum file_type type;

	uint_fast32_t dict_size;
	uint_fast32_t dict_base;
	sdict_t *dict_words;
	char **dict_text;
} save_t;

static enum size_type data_size(uint64_t data) {
	if (data < UINT8_MAX) return SZ_8;
	else if (data < UINT16_MAX) return SZ_16;
	else if (data < UINT32_MAX) return SZ_32;
	else return SZ_64;
}

static int write_data(save_t *data, enum size_type size, uint64_t value) {
	switch (size) {
	case SZ_8: {
			uint8_t tmp = value;
			if (!fwrite(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
		}
		break;

	case SZ_16: {
			uint16_t tmp;
			if (data->type == FILETYPE_MEGAHAL8) tmp = value;
			else tmp = htobe16(value);
			if (!fwrite(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
		}
		break;

	case SZ_32: {
			uint32_t tmp;
			if (data->type == FILETYPE_MEGAHAL8) tmp = value;
			else tmp = htobe32(value);
			if (!fwrite(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
		}
		break;

	case SZ_64: {
			uint64_t tmp;
			if (data->type == FILETYPE_MEGAHAL8) tmp = value;
			else tmp = htobe64(value);
			if (!fwrite(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
		}
		break;

	default:
		BUG();
	}

	return OK;
}

static inline int read_data(load_t *data, enum size_type size, uint64_t *value) {
	switch (size) {
	case SZ_8: {
			uint8_t tmp;
			if (!fread(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
			*value = tmp;
		}
		break;

	case SZ_16: {
			uint16_t tmp;
			if (!fread(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
			if (data->type == FILETYPE_MEGAHAL8) *value = tmp;
			else *value = be16toh(tmp);
		}
		break;

	case SZ_32: {
			uint32_t tmp;
			if (!fread(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
			if (data->type == FILETYPE_MEGAHAL8) *value = tmp;
			else *value = be32toh(tmp);
		}
		break;

	case SZ_64: {
			uint64_t tmp;
			if (!fread(&tmp, sizeof(tmp), 1, data->fd)) return -EIO;
			if (data->type == FILETYPE_MEGAHAL8) *value = tmp;
			else *value = be64toh(tmp);
		}
		break;

	default:
		BUG();
	}

	return OK;
}

static int load_tree(load_t *data, db_tree *tree) {
	uint64_t symbol;
	uint64_t usage;
	uint64_t count;
	uint64_t branch;
	int ret;
	uint8_t sizes;
	uint_fast16_t i;

	WARN_IF(data == NULL);

	switch (data->type) {
	case FILETYPE_MEGAHAL8:
		ret = read_data(data, SZ_16, &symbol);
		if (ret) return ret;

		ret = read_data(data, SZ_32, &usage);
		if (ret) return ret;

		ret = read_data(data, SZ_16, &count);
		if (ret) return ret;

		ret = read_data(data, SZ_16, &branch);
		if (ret) return ret;
		break;

	case FILETYPE_SQLHAL0:
		if (!fread(&sizes, sizeof(sizes), 1, data->fd)) return -EIO;

		ret = read_data(data, (sizes >> 6) & 3, &symbol);
		if (ret) return ret;

		/* FIN token implies no children or usage */
		if (symbol == TOKEN_FIN_IDX) {
			branch = 0;
			usage = 0;

			/* count stored in sizes byte */
			if (((sizes >> 5) & 1) == 1) {
				count = (sizes & 31) + 1;
			} else if (((sizes >> 4) & 1) == 1) {
				count = (sizes & 15) + 33;
			} else if (((sizes >> 3) & 1) == 1) {
				count = (sizes & 7) + 49;
			} else if (((sizes >> 2) & 1) == 1) {
				count = (sizes & 3) + 57;
			} else {
				ret = read_data(data, sizes & 3, &count);
				if (ret) return ret;
			}
		} else {
			ret = read_data(data, (sizes >> 4) & 3, &branch);
			if (ret) return ret;

			/* no branches implies no usage */
			if (branch > 0) {
				ret = read_data(data, (sizes >> 2) & 3, &usage);
				if (ret) return ret;

				/* ERROR token implies no count */
				if (symbol != TOKEN_ERROR_IDX) {
					ret = read_data(data, sizes & 3, &count);
					if (ret) return ret;
				} else {
					count = 0;
				}
			} else {
				usage = 0;

				/* ERROR token implies no count */
				if (symbol != TOKEN_ERROR_IDX) {
					/* count stored in sizes byte */
					if (((sizes >> 3) & 1) == 1) {
						count = (sizes & 7) + 1;
					} else if (((sizes >> 2) & 1) == 1) {
						count = (sizes & 3) + 9;
					} else {
						ret = read_data(data, sizes & 3, &count);
						if (ret) return ret;
					}
				} else {
					count = 0;
				}
			}
		}
		break;

	default:
		BUG();
	}

	if (data->mode == LOAD_STORE) {
		WARN_IF(data->dict_words == NULL);
		WARN_IF(data->brain == 0);
		WARN_IF(tree == NULL);

		if (symbol >= data->dict_size) {
			log_error("load_tree", symbol, "Symbol references beyond end of dictionary");
			WARN();
		}

		tree->word = data->dict_words[symbol];
		tree->usage = usage;
		tree->count = count;

		ret = db_model_update(data->brain, tree);
		if (ret) return ret;
	}

	for (i = 0; i < branch; i++) {
		db_tree *node = NULL;

		if (data->mode == LOAD_STORE) {
			node = db_model_node_alloc();
			if (node == NULL) return -ENOMEM;

			ret = db_model_link(tree, node);
			if (ret) return ret;
		}

		ret = load_tree(data, node);
		if (ret) return ret;

		if (data->mode == LOAD_STORE) {
			db_model_node_free(&node);
		}
	}

	return OK;
}

static int load_dict(load_t *data) {
	uint64_t size;
	uint8_t length;
	int ret;
	word_t word;
	char tmp[256];
	uint_fast32_t i;

	switch (data->type) {
	case FILETYPE_MEGAHAL8:
		ret = read_data(data, SZ_32, &size);
		if (ret) return ret;

		i = 0;
		break;

	case FILETYPE_SQLHAL0:
		ret = read_data(data, SZ_64, &size);
		if (ret) return ret;

		//i = TOKENS;
		break;

	default:
		BUG();
	}

	if (size > UINT32_MAX) {
		log_error("load_dict", size, "Cannot handle brains with more than 2^32-1 words");
		WARN();
	}

	data->dict_size = size;
	data->dict_words = malloc(sizeof(word_t) * size);
	if (data->dict_words == NULL) return -ENOMEM;

	if (data->type == FILETYPE_SQLHAL0) {
		for (i = 0; i < TOKENS; i++)
			data->dict_words[i] = 0;
	}

	for (; i < data->dict_size; i++) {
		if (!fread(&length, sizeof(length), 1, data->fd)) return -EIO;

		tmp[length] = 0;
		if (fread(tmp, sizeof(char), length, data->fd) != length) return -EIO;

		switch (i) {
		case TOKEN_ERROR_IDX:
			if (strcmp(tmp, TOKEN_ERROR)) {
				log_error("load_dict", i, "Invalid word (not " TOKEN_ERROR ")");
				WARN();
			}
			data->dict_words[i] = 0;
			break;

		case TOKEN_FIN_IDX:
			if (strcmp(tmp, TOKEN_FIN)) {
				log_error("load_dict", i, "Invalid word (not " TOKEN_FIN ")");
				WARN();
			}
			data->dict_words[i] = 0;
			break;

		default:
			ret = db_word_use(tmp, &word);
			if (ret) return ret;

			data->dict_words[i] = word;
		}
	}

	return OK;
}

static void free_loaded_dict(load_t *data) {
	free(data->dict_words);

	data->dict_size = 0;
	data->dict_words = NULL;
}

static void free_saved_dict(save_t *data) {
	uint_fast32_t i;

	if (data->dict_text != NULL) {
		for (i = TOKENS; i < data->dict_size; i++)
			free(data->dict_text[i]);
	}

	free(data->dict_words);
	free(data->dict_text);

	data->dict_size = 0;
	data->dict_words = NULL;
	data->dict_text = NULL;
}

static int save_dict(save_t *data) {
	int ret;
	size_t len;
	uint8_t tmp8;
	uint_fast32_t i;

	switch (data->type) {
	case FILETYPE_MEGAHAL8:
		ret = write_data(data, SZ_32, data->dict_size);
		if (ret) return ret;
		break;

	case FILETYPE_SQLHAL0:
		BUG();

	default:
		BUG();
	}

	for (i = 0; i < data->dict_size; i++) {
		len = strlen(data->dict_text[i]);
		if (len > UINT8_MAX) return -ENOSPC;
		tmp8 = len;
		if (!fwrite(&tmp8, sizeof(tmp8), 1, data->fd)) return -EIO;
		if (fwrite(data->dict_text[i], sizeof(char), len, data->fd) != len) return -EIO;
	}

	return OK;
}

static int read_dict_size(void *data_, number_t size) {
	save_t *data = data_;
	int ret;
	void *mem;

	if ((data->dict_base + size) > UINT32_MAX || (data->dict_base + size) < data->dict_base)
		return -ENOSPC;

	if (data->type == FILETYPE_SQLHAL0) {
		ret = write_data(data, SZ_64, data->dict_base + size);
		if (ret) return ret;
	}

	mem = realloc(data->dict_words, sizeof(sdict_t) * (data->dict_base + size));
	if (mem == NULL) return -ENOMEM;
	data->dict_words = mem;

	if (data->type == FILETYPE_SQLHAL0) {
		free(data->dict_text);
		data->dict_text = NULL;
	} else {
		mem = realloc(data->dict_text, sizeof(char *) * (data->dict_base + size));
		if (mem == NULL) return -ENOMEM;
		data->dict_text = mem;
	}

	return OK;
}

static int read_dict_iter(void *data_, word_t word, number_t pos, const char *text) {
	save_t *data = data_;

	if (data->dict_size >= UINT32_MAX) return -ENOSPC;

	data->dict_words[data->dict_base + pos].word = word;
	data->dict_words[data->dict_base + pos].idx = data->dict_size;

	if (data->type == FILETYPE_SQLHAL0) {
		uint8_t length = strlen(text);
		if (!fwrite(&length, sizeof(length), 1, data->fd)) return -EIO;
		if (fwrite(text, sizeof(char), length, data->fd) != length) return -EIO;
	} else {
		data->dict_text[data->dict_size] = strdup(text);
		if (data->dict_text[data->dict_size] == NULL) return -ENOMEM;
	}

	data->dict_size++;
	return OK;
}

static int init_dict(save_t *data) {
	data->dict_size = 0;

	data->dict_words = malloc(sizeof(sdict_t) * TOKENS);
	if (data->dict_words == NULL) return -ENOMEM;

	data->dict_text = malloc(sizeof(char *) * TOKENS);
	if (data->dict_text == NULL) return -ENOMEM;

	data->dict_words[TOKEN_ERROR_IDX].word = 0;
	data->dict_words[TOKEN_ERROR_IDX].idx = TOKEN_ERROR_IDX;
	data->dict_text[TOKEN_ERROR_IDX] = TOKEN_ERROR;

	data->dict_words[TOKEN_FIN_IDX].word = 0;
	data->dict_words[TOKEN_FIN_IDX].idx = TOKEN_FIN_IDX;
	data->dict_text[TOKEN_FIN_IDX] = TOKEN_FIN;

	data->dict_size = TOKENS;
	data->dict_base = data->dict_size;

	return OK;
}

static int read_dict(save_t *data) {
	int ret;

	ret = db_model_dump_words(data->brain, read_dict_size, read_dict_iter, data);
	if (ret) {
		free_saved_dict(data);
		return ret;
	}

	return OK;
}

static int find_word(save_t *data, word_t word, uint32_t *symbol) {
	uint_fast32_t min = 0;
	uint_fast32_t pos;
	uint_fast32_t max = data->dict_size - 1;

	WARN_IF(word == 0);

	while (1) {
		pos = (min + max) / 2;

		if (word == data->dict_words[pos].word) {
			BUG_IF(pos == TOKEN_ERROR_IDX);
			BUG_IF(pos == TOKEN_FIN_IDX);
			*symbol = data->dict_words[pos].idx;
			return OK;
		} else if (word > data->dict_words[pos].word) {
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

static int save_tree(save_t *data, db_tree **tree) {
	db_tree *tree_p;
	int ret;
	uint32_t word;
	uint_fast32_t i;

	WARN_IF(data == NULL);
	WARN_IF(data->brain == 0);
	WARN_IF(tree == NULL);
	WARN_IF(*tree == NULL);

	if (data->dict_size > UINT16_MAX)
		return -ENOSPC;

	tree_p = *tree;

	ret = db_model_node_fill(data->brain, tree_p);
	if (ret) return ret;

	if (tree_p->word == 0) {
		if (tree_p->parent_id == 0) {
			BUG_IF(tree_p->count != 0);

			word = TOKEN_ERROR_IDX;
		} else {
			BUG_IF(tree_p->usage != 0);
			BUG_IF(tree_p->children != 0);

			word = TOKEN_FIN_IDX;
		}
	} else {
		ret = find_word(data, tree_p->word, &word);
		if (ret) return ret;
	}

	switch (data->type) {
	case FILETYPE_MEGAHAL8: {
			uint16_t symbol;
			uint32_t usage;
			uint16_t count;
			uint16_t branch;

			BUG_IF(word > UINT16_MAX);
			symbol = word;
			usage = tree_p->usage > UINT32_MAX ? UINT32_MAX : tree_p->usage;
			count = tree_p->count > UINT16_MAX ? UINT16_MAX : tree_p->count;

			if (tree_p->children > UINT16_MAX)
				return -ENOSPC;
			branch = tree_p->children;

			if (!fwrite(&symbol, sizeof(symbol), 1, data->fd)) return -EIO;
			if (!fwrite(&usage, sizeof(usage), 1, data->fd)) return -EIO;
			if (!fwrite(&count, sizeof(count), 1, data->fd)) return -EIO;
			if (!fwrite(&branch, sizeof(branch), 1, data->fd)) return -EIO;
		}
		break;

	case FILETYPE_SQLHAL0: {
			uint8_t sizes = (data_size(word) << 6)
				| (data_size(tree_p->children) << 4)
				| (data_size(tree_p->usage) << 2)
				| data_size(tree_p->count);

			if (tree_p->children == 0) {
				BUG_IF(tree_p->usage != 0);

				if (word == TOKEN_FIN_IDX) {
					/* no children and 0 < count <= 32, store in sizes byte */
					if (tree_p->count <= 32 && tree_p->count > 0) {
						sizes = (sizes & 0xc0) | (1 << 5) | (tree_p->count - 1);

					/* no children and 0 < count <= 48, store in sizes byte */
					} else if (tree_p->count <= 48 && tree_p->count > 0) {
						sizes = (sizes & 0xc0) | (1 << 4) | (tree_p->count - 33);

					/* no children and 0 < count <= 56, store in sizes byte */
					} else if (tree_p->count <= 56 && tree_p->count > 0) {
						sizes = (sizes & 0xc0) | (1 << 3) | (tree_p->count - 49);

					/* no children and 0 < count <= 60, store in sizes byte */
					} else if (tree_p->count <= 60 && tree_p->count > 0) {
						sizes = (sizes & 0xc0) | (1 << 2) | (tree_p->count - 57);

					} else {
						/* bits ((sizes >> 2) & 7) == 0 */
					}
				} else {
					/* no children and 0 < count <= 8, store in sizes byte */
					if (tree_p->count <= 8 && tree_p->count > 0) {
						sizes = (sizes & 0xf0) | (1 << 3) | (tree_p->count - 1);

					/* no children and 0 < count <= 12, store in sizes byte */
					} else if (tree_p->count <= 12 && tree_p->count > 0) {
						sizes = (sizes & 0xf0) | (1 << 2) | (tree_p->count - 9);

					} else {
						/* bits ((sizes >> 4) & 3) == 0 */
					}
				}
			}

			if (!fwrite(&sizes, sizeof(sizes), 1, data->fd)) return -EIO;

			ret = write_data(data, data_size(word), word);
			if (ret) return ret;

			/* FIN token implies no children or usage */
			if (word != TOKEN_FIN_IDX) {
				ret = write_data(data, data_size(tree_p->children), tree_p->children);
				if (ret) return ret;

				/* no children implies no usage */
				if (tree_p->children > 0) {
					ret = write_data(data, data_size(tree_p->usage), tree_p->usage);
					if (ret) return ret;
				}
			}

			if (word == TOKEN_ERROR_IDX) {
				/* ERROR token implies no count */
			} else if (word == TOKEN_FIN_IDX) {
				/* no children and 0 < count <= 32, stored in sizes byte */
				if (tree_p->children > 0 || tree_p->count > 32) {
					ret = write_data(data, data_size(tree_p->count), tree_p->count);
					if (ret) return ret;
				}
			} else {
				/* no children and 0 < count <= 12, stored in sizes byte */
				if (tree_p->children > 0 || tree_p->count > 12) {
					ret = write_data(data, data_size(tree_p->count), tree_p->count);
					if (ret) return ret;
				}
			}
		}
		break;

	default:
		BUG();
	}

	for (i = 0; i < tree_p->children; i++) {
		ret = save_tree(data, (db_tree **)&tree_p->nodes[i]);
		if (ret) return ret;
	}
	db_model_node_free(tree);

	return OK;
}

int load_brain(const char *name, const char *filename) {
	int ret = OK;
	load_t data;
	char cookie[COOKIE_LEN];
	uint8_t tmp8;
	db_tree *forward;
	db_tree *backward;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	log_info("load_brain", 0, filename);

	data.fd = fopen(filename, "r");
	if (data.fd == NULL) return -EIO;

	ret = db_brain_use(name, &data.brain);
	if (ret) goto fail;

	ret = db_model_zap(data.brain);
	if (ret) goto fail;

	if (fread(cookie, sizeof(char), COOKIE_LEN, data.fd) != COOKIE_LEN) return -EIO;
	if (strncmp(cookie, COOKIE_M8, COOKIE_LEN) == 0) {
		data.type = FILETYPE_MEGAHAL8;
	} else if (strncmp(cookie, COOKIE_S0, COOKIE_LEN) == 0) {
		data.type = FILETYPE_SQLHAL0;
	} else {
		log_error("load_brain", 1, "Not a MegaHAL brain");
		ret = -EIO;
		goto fail;
	}

	if (!fread(&tmp8, sizeof(tmp8), 1, data.fd)) return -EIO;
	data.order = tmp8;

	ret = db_model_set_order(data.brain, data.order);
	if (ret) goto fail;

	ret = db_model_get_root(data.brain, &forward, &backward);
	if (ret) goto fail;

	if (data.type == FILETYPE_MEGAHAL8) {
		/* Bah. The word dictionary is at the end of the file.
		 * Either the file can be read twice or we can waste a ton of memory caching the tree.
		 */
		data.mode = LOAD_IGNORE;
		ret = load_tree(&data, NULL); /* forward */
		if (ret) goto fail;

		log_info("load_brain", 0, "Skipped forward tree");

		ret = load_tree(&data, NULL); /* backward */
		if (ret) goto fail;

		log_info("load_brain", 0, "Skipped backward tree");
	}

	ret = load_dict(&data);
	if (ret) goto fail;

	log_info("load_brain", data.dict_size, "Dictionary loaded");

	data.mode = LOAD_STORE;

	if (data.type == FILETYPE_MEGAHAL8) {
		/* Read most of the file again... */
		if (fseek(data.fd, sizeof(char) * COOKIE_LEN + sizeof(tmp8), SEEK_SET)) return -EIO;
	}

	ret = load_tree(&data, forward);
	if (ret) goto fail;

	db_model_node_free(&forward);

	log_info("load_brain", 0, "Forward tree loaded");

	ret = load_tree(&data, backward);
	if (ret) goto fail;

	db_model_node_free(&backward);

	log_info("load_brain", 0, "Backward tree loaded");

	free_loaded_dict(&data);

fail:
	fclose(data.fd);
	return ret;
}

int save_brain(const char *name, enum file_type type, const char *filename) {
	int ret = OK;
	save_t data;
	const char *cookie;
	uint8_t tmp8;
	db_tree *forward;
	db_tree *backward;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	switch (type) {
	case FILETYPE_MEGAHAL8:
		cookie = COOKIE_M8;
		break;

	case FILETYPE_SQLHAL0:
		cookie = COOKIE_S0;
		break;

	default:
		BUG();
	}

	log_info("save_brain", 0, filename);

	data.type = type;
	data.fd = fopen(filename, "w");
	if (data.fd == NULL) return -EIO;

	ret = db_brain_get(name, &data.brain);
	if (ret) goto fail;

	ret = db_model_get_order(data.brain, &data.order);
	if (ret) goto fail;

	if (data.order > UINT8_MAX) return -ENOSPC;
	tmp8 = data.order;

	ret = db_model_get_root(data.brain, &forward, &backward);
	if (ret) goto fail;

	if (fwrite(cookie, sizeof(char), COOKIE_LEN, data.fd) != COOKIE_LEN) return -EIO;
	if (!fwrite(&tmp8, sizeof(tmp8), 1, data.fd)) return -EIO;

	ret = init_dict(&data);
	if (ret) goto fail;

	switch (data.type) {
	case FILETYPE_MEGAHAL8:
		ret = read_dict(&data);
		if (ret) goto fail;

		log_info("save_brain", data.dict_size, "Dictionary read");

		ret = save_tree(&data, &forward); /* forward */
		if (ret) goto fail;

		log_info("save_brain", 0, "Forward tree saved");

		ret = save_tree(&data, &backward); /* backward */
		if (ret) goto fail;

		log_info("save_brain", 0, "Backward tree saved");

		ret = save_dict(&data);
		if (ret) goto fail;

		log_info("save_brain", data.dict_size, "Dictionary saved");
		break;

	case FILETYPE_SQLHAL0:
		ret = read_dict(&data);
		if (ret) goto fail;

		log_info("save_brain", data.dict_size, "Dictionary saved");

		ret = save_tree(&data, &forward); /* forward */
		if (ret) goto fail;

		log_info("save_brain", 0, "Forward tree saved");

		ret = save_tree(&data, &backward); /* backward */
		if (ret) goto fail;

		log_info("save_brain", 0, "Backward tree saved");
		break;

	default:
		BUG();
	}

	free_saved_dict(&data);

fail:
	fclose(data.fd);
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

	model_p->contexts = malloc(sizeof(db_tree *) * (model_p->order + 2));
	if (model_p->contexts == NULL) { ret = -ENOMEM; goto fail; }
	for (i = 0; i < model_p->order + 2; i++)
		model_p->contexts[i] = NULL;

	return OK;

fail:
	if (model_p->contexts != NULL)
		free(model_p->contexts);

	free(*model);
	*model = NULL;
	return ret;
}

int model_init(model_t *model, enum model_dir dir) {
	uint_fast32_t i;
	db_tree *tmp;
	int ret;

	BUG_IF(model == NULL);

	for (i = 0; i < model->order + 2; i++)
		db_model_node_free(&model->contexts[i]);

	switch (dir) {
	case MODEL_FORWARD:
		ret = db_model_get_root(model->brain, &model->contexts[0], &tmp);
		if (ret) return ret;
		break;

	case MODEL_BACKWARD:
		ret = db_model_get_root(model->brain, &tmp, &model->contexts[0]);
		if (ret) return ret;
		break;

	default:
		BUG();
	}
	db_model_node_free(&tmp);

	return OK;
}

int model_update(model_t *model, word_t word, int persist) {
	uint_fast32_t i;
	int ret;

	BUG_IF(model == NULL);

	for (i = model->order + 1; i > 0; i--)
		if (model->contexts[i - 1] != NULL) {
			ret = db_model_node_find(model->brain, model->contexts[i - 1], word, &model->contexts[i]);
			if (ret == -ENOTFOUND) {
				db_model_node_free(&model->contexts[i]);
			} else if (ret != OK) {
				return ret;
			}

			if (persist) {
				if (ret == -ENOTFOUND) {
					model->contexts[i] = db_model_node_alloc();
					if (model->contexts[i] == NULL) return -ENOMEM;

					model->contexts[i]->word = word;

					ret = db_model_link(model->contexts[i - 1], model->contexts[i]);
					if (ret) return ret;

					model->contexts[i]->count = 1;

					ret = db_model_update(model->brain, model->contexts[i]);
					if (ret) return ret;
				} else {
					if (model->contexts[i]->count < (number_t)~0)
						model->contexts[i]->count++;

					ret = db_model_update(model->brain, model->contexts[i]);
					if (ret) return ret;
				}

				if (model->contexts[i - 1]->usage < (number_t)~0) {
					model->contexts[i - 1]->usage++;

					ret = db_model_update(model->brain, model->contexts[i - 1]);
					if (ret) return ret;
				}
			}
		}

	return OK;
}

void model_free(model_t **model) {
	model_t *model_p;
	uint_fast32_t i;

	if (*model == NULL) return;
	model_p = *model;

	for (i = 0; i < model_p->order + 2; i++)
		db_model_node_free(&model_p->contexts[i]);
	free(model_p->contexts);

	free(*model);
	*model = NULL;
}
