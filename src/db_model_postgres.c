#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "output.h"

#include "db_postgres.h"

int db_model_get_order(brain_t brain, number_t *order) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];

	if (brain == 0 || order == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	GET_VALUE(res, 0, 0, *order);

	PQclear(res);

	return OK;

fail:
	log_error("db_model_get_order", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_set_order(brain_t brain, number_t order) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];
	number_t current;
	int ret;

	if (brain == 0 || order == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, order);

	ret = db_model_get_order(brain, &current);
	if (ret == -ENOTFOUND) {
		res = PQexecPrepared(conn, "model_add", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else if (!ret) {
		res = PQexecPrepared(conn, "model_set", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else {
		return ret;
	}

	return OK;

fail:
	log_error("db_model_set_order", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_zap(brain_t brain) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];

	if (brain == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_zap", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_model_zap", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

db_tree *db_model_node_alloc(void) {
	db_tree *node;

	node = malloc(sizeof(db_tree));
	if (node == NULL) return NULL;

	node->id = 0;
	node->parent_id = 0;
	node->word = 0;
	node->usage = 0;
	node->count = 0;

	node->children = 0;
	node->nodes = NULL;

	return node;
}

void db_model_node_free(db_tree **node) {
	db_tree *node_p;
	number_t i;

	if (node == NULL || *node == NULL) return;

	node_p = *node;

	for (i = 0; i < node_p->children; i++)
		db_model_node_free((db_tree **)&node_p->nodes[i++]);
	free(node_p->nodes);

	free(*node);
	*node = NULL;
}

int db_model_node_fill(brain_t brain, db_tree *node) {
	PGresult *res;
	unsigned int num, pos, i;
	const char *param[2];
	char tmp[2][32];
	int parent;

	if (brain == 0 || node == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, node->id);

	res = PQexecPrepared(conn, "model_node_get", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	num = PQntuples(res);
	if (num == 0) goto not_found;

	if (node->nodes != NULL) {
		for (i = 0; i < node->children; i++)
			db_model_node_free((db_tree **)&node->nodes[i++]);
		free(node->nodes);
		node->children = 0;
	}

	node->children = num - 1;

	if (node->children > 0) {
		node->nodes = malloc(sizeof(db_tree *) * node->children);
		if (node->nodes == NULL) {
			PQclear(res);

			node->children = 0;

			return -ENOMEM;
		}
	}

	parent = -EFAULT;
	for (i = 0, pos = 0; i < num; i++) {
		db_tree *child;
		node_t id;

		GET_VALUE(res, i, 0, id);

		if (id == node->id) {
			parent = OK;

			GET_VALUE(res, i, 1, node->word);
			GET_VALUE(res, i, 2, node->usage);
			GET_VALUE(res, i, 3, node->count);
		} else {
			node->nodes[pos] = db_model_node_alloc();
			if (node->nodes[pos] == NULL) {
				PQclear(res);

				while (pos > 0)
					db_model_node_free((db_tree **)&node->nodes[--pos]);
				free(node->nodes);
				node->nodes = NULL;

				return -ENOMEM;
			}

			child = (db_tree *)node->nodes[pos];

			child->id = id;
			GET_VALUE(res, i, 1, child->word);
			GET_VALUE(res, i, 2, child->usage);
			GET_VALUE(res, i, 3, child->count);

			pos++;
		}
	}

	PQclear(res);

	if (parent)
		return parent;

	return OK;

fail:
	log_error("db_model_node_fill", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	log_error("db_model_node_fill", node->id, "Node not found");
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_get_root(brain_t brain, db_tree **forward, db_tree **backward) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];
	db_tree *forward_p;
	db_tree *backward_p;
	int created = 0;
	int ret;

	if (brain == 0 || forward == NULL || backward == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	*forward = NULL;
	*backward = NULL;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_root_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto fail;

	if (!PQgetisnull(res, 0, 0)) {
		*forward = db_model_node_alloc();
		if (*forward == NULL) return -ENOMEM;
		forward_p = *forward;

		GET_VALUE(res, 0, 0, forward_p->id);

		ret = db_model_node_fill(brain, forward_p);
		if (ret) { PQclear(res); return ret; }
	} else {
		ret = db_model_create(brain, forward);
		if (ret) { PQclear(res); return ret; }

		forward_p = *forward;
		created = 1;
	}

	if (!PQgetisnull(res, 0, 1)) {
		*backward = db_model_node_alloc();
		if (*backward == NULL) return -ENOMEM;
		backward_p = *backward;

		GET_VALUE(res, 0, 1, backward_p->id);

		ret = db_model_node_fill(brain, backward_p);
		if (ret) { PQclear(res); return ret; }
	} else {
		ret = db_model_create(brain, backward);
		if (ret) { PQclear(res); return ret; }

		backward_p = *backward;
		created = 1;
	}
	PQclear(res);

	if (created) {
		SET_PARAM(param, tmp, 1, forward_p->id);
		SET_PARAM(param, tmp, 2, backward_p->id);

		res = PQexecPrepared(conn, "model_root_set", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_get_root", PQresultStatus(res), PQresultErrorMessage(res));
	db_model_node_free(forward);
	db_model_node_free(backward);
	PQclear(res);
	return -EDB;
}

int db_model_create(brain_t brain, db_tree **node) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];
	db_tree *node_p;

	if (brain == 0 || node == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	*node = db_model_node_alloc();
	if (*node == NULL) return -ENOMEM;
	node_p = *node;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_create", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	GET_VALUE(res, 0, 0, node_p->id);

	PQclear(res);

	return OK;

fail:
	log_error("db_model_create", PQresultStatus(res), PQresultErrorMessage(res));
	free(*node);
	PQclear(res);
	return -EDB;
}

int db_model_update(brain_t brain, db_tree *node) {
	PGresult *res;
	const char *param[5];
	char tmp[5][32];

	if (brain == 0 || node == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	if (node->id == 0) {
		SET_PARAM(param, tmp, 0, brain);
	} else {
		SET_PARAM(param, tmp, 0, node->id);
	}

	SET_PARAM(param, tmp, 1, node->usage);
	SET_PARAM(param, tmp, 2, node->count);

	if (node->parent_id == 0) {
		res = PQexecPrepared(conn, "model_rootupdate", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else if (node->id == 0) {
		if (node->word == 0) {
			param[3] = NULL;
		} else {
			SET_PARAM(param, tmp, 3, node->word);
		}
		SET_PARAM(param, tmp, 4, node->parent_id);

		res = PQexecPrepared(conn, "model_fastcreate", 5, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
		if (PQntuples(res) != 1) goto fail;

		GET_VALUE(res, 0, 0, node->id);

		PQclear(res);
	} else {
		res = PQexecPrepared(conn, "model_update", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_update", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_link(db_tree *parent, db_tree *child) {
	if (parent == NULL || child == NULL) return -EINVAL;
	if (parent->id == 0) return -EINVAL;
	if (child->parent_id != 0) return -EINVAL;
	if (parent->parent_id != 0 && parent->word == 0) return -EINVAL;

	child->parent_id = parent->id;
	return OK;
}

int db_model_dump_words(brain_t brain, uint_fast32_t *dict_size, word_t **dict_words, uint32_t **dict_idx, char ***dict_text) {
	PGresult *res;
	unsigned int num, i;
	uint_fast32_t base;
	void *mem;
	const char *param[1];
	char tmp[1][32];

	if (brain == 0 || dict_size == NULL || dict_words == NULL || dict_idx == NULL || dict_text == NULL) return -EINVAL;
	if (*dict_words == NULL || *dict_idx == NULL || *dict_text == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_brain_words", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	base = *dict_size;
	num = PQntuples(res);

	mem = realloc(*dict_words, sizeof(word_t) * (base + num));
	if (mem == NULL) { PQclear(res); return -ENOMEM; }
	*dict_words = mem;

	mem = realloc(*dict_idx, sizeof(uint32_t) * (base + num));
	if (mem == NULL) { PQclear(res); return -ENOMEM; }
	*dict_idx = mem;

	mem = realloc(*dict_text, sizeof(char *) * (base + num));
	if (mem == NULL) { PQclear(res); return -ENOMEM; }
	*dict_text = mem;

	for (i = 0; i < num; i++) {
		word_t word;
		number_t pos;
		char *text;

		GET_VALUE(res, i, 0, word);
		GET_VALUE(res, i, 1, pos);
		text = PQgetvalue(res, i, 2);

		(*dict_words)[base + pos] = word;
		(*dict_idx)[base + pos] = *dict_size;
		(*dict_text)[*dict_size] = strdup(text);
		if ((*dict_text)[*dict_size] == NULL) {
			PQclear(res);
			return -ENOMEM;
		}

		(*dict_size)++;
		if (*dict_size <= 0 || *dict_size > UINT32_MAX) {
			PQclear(res);
			return -ENOSPC;
		}
	}

	PQclear(res);

	return OK;

fail:
	log_error("db_model_dump_words", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}
