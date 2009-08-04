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

int db_model_init(db_hand **hand, brain_t brain) {
	PGresult *res;
	const char *models[] = { "models" };
	const char *nodes[] = { "nodes" };
	int nodes_created = 0;
	struct db_hand_postgres *hand_p;
	int ret;

	if (hand == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	*hand = NULL;

	res = PQexecPrepared(conn, "table_exists", 1, nodes, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) {
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE nodes (id BIGSERIAL UNIQUE, brain BIGINT NOT NULL, parent BIGINT, word BIGINT, usage BIGINT NOT NULL, count BIGINT NOT NULL,"\
			" PRIMARY KEY (brain, id),"\
			" FOREIGN KEY (parent) REFERENCES nodes (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
			" FOREIGN KEY (word) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
			" CONSTRAINT valid_id CHECK (id > 0),"\
			" CONSTRAINT valid_usage CHECK (usage >= 0),"\
			" CONSTRAINT valid_count CHECK (count >= 0))");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexec(conn, "CREATE INDEX nodes_brain ON nodes (brain)");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexec(conn, "CREATE INDEX nodes_words ON nodes (word)");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;

		res = PQexec(conn, "CREATE INDEX nodes_child ON nodes (parent)");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		nodes_created = 1;
	}
	PQclear(res);

	res = PQexecPrepared(conn, "table_exists", 1, models, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) {
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE models (brain BIGINT NOT NULL, contexts BIGINT NOT NULL, forward BIGINT, backward BIGINT,"\
			" PRIMARY KEY (brain),"\
			" FOREIGN KEY (brain) REFERENCES brains (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
			" FOREIGN KEY (forward) REFERENCES nodes (id),"\
			" FOREIGN KEY (backward) REFERENCES nodes (id),"\
			" CONSTRAINT valid_order CHECK (contexts >= 0))");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	}
	PQclear(res);

	if (nodes_created) {
		res = PQexec(conn, "ALTER TABLE nodes"\
			" ADD FOREIGN KEY (brain) REFERENCES models (brain) ON UPDATE CASCADE ON DELETE CASCADE");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	ret = db_hand_init(hand);
	if (ret) return ret;
	hand_p = *hand;

	hand_p->brain = malloc(21 * sizeof(char));
	if (hand_p->brain == NULL) { ret = -ENOMEM; goto fail_free; }
	if (sizeof(number_t) == sizeof(unsigned long int)) {
		if (sprintf(hand_p->brain, "%lu", (unsigned long int)brain) <= 0) { ret = -ENOMEM; goto fail_free; }
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		if (sprintf(hand_p->brain, "%llu", (unsigned long long int)brain) <= 0) { ret = -ENOMEM; goto fail_free; }
	} else {
		return -EFAULT;
	}

	res = PQprepare(conn, "model_add", "INSERT INTO models (brain, contexts) VALUES($1, $2)", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_get", "SELECT contexts FROM models WHERE brain = $1", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_set", "UPDATE models SET contexts = $2 WHERE brain = $1", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_zap", "DELETE FROM models WHERE brain = $1", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_create", "INSERT INTO nodes (brain, usage, count) VALUES($1, 0, 0)", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_fastcreate", "INSERT INTO nodes (brain, usage, count, word, parent) VALUES($1, $2, $3, $4, $5)", 5, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_create_id", "SELECT currval('nodes_id_seq')", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_rootupdate", "UPDATE nodes SET parent = NULL, usage = $2, count = $3, word = $4 WHERE id = $1", 4, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_update", "UPDATE nodes SET usage = $2, count = $3 WHERE id = $1", 3, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_root_get", "SELECT forward, backward FROM models WHERE brain = $1", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_node_get", "SELECT word, usage, count FROM nodes WHERE brain = $1 AND id = $2", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_root_set", "UPDATE models SET forward = $2, backward = $3 WHERE brain = $1", 3, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_model_init", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	ret = -EDB;
fail_free:
	if (*hand != NULL) {
		free(hand_p->brain);
		free(*hand);
		*hand = NULL;
	}
	return ret;
}

int db_model_free(db_hand **hand) {
	PGresult *res;

    res = PQexec(conn, "DEALLOCATE PREPARE model_add");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_set");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_zap");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_create");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_fastcreate");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_create_id");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_rootupdate");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_update");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_root_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_node_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_root_set");
    PQclear(res);

	return db_hand_free(hand);
}

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
	node->nodes = NULL;

	return node;
}

int db_model_node_fill(brain_t brain, db_tree *node) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];

	if (brain == 0 || node == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, node->id);

	res = PQexecPrepared(conn, "model_node_get", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	GET_VALUE(res, 0, 0, node->word);
	GET_VALUE(res, 0, 1, node->usage);
	GET_VALUE(res, 0, 2, node->count);

	PQclear(res);

	return OK;

fail:
	log_error("db_model_node_fill", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
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
		if (ret) goto fail;
	} else {
		ret = db_model_create(brain, forward);
		if (ret) goto fail;

		forward_p = *forward;
		created = 1;
	}

	if (!PQgetisnull(res, 0, 1)) {
		*backward = db_model_node_alloc();
		if (*backward == NULL) return -ENOMEM;
		backward_p = *backward;

		GET_VALUE(res, 0, 1, backward_p->id);

		ret = db_model_node_fill(brain, backward_p);
		if (ret) goto fail;
	} else {
		ret = db_model_create(brain, backward);
		if (ret) goto fail;

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

	if (node->id == 0 || node->parent_id == 0) {
		SET_PARAM(param, tmp, 3, node->word);
	}

	if (node->id == 0) {
		SET_PARAM(param, tmp, 4, node->parent_id);
	}

	if (node->id == 0) {
		res = PQexecPrepared(conn, "model_fastcreate", 5, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
		if (PQntuples(res) != 1) goto fail;

		GET_VALUE(res, 0, 0, node->id);

		PQclear(res);
	} else if (node->parent_id == 0) {
		res = PQexecPrepared(conn, "model_rootupdate", 4, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
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

	child->parent_id = parent->id;
	return OK;
}

void db_model_node_free(db_tree **node) {
	db_tree *node_p;
	number_t i = 0;

	if (node == NULL || *node == NULL) return;

	node_p = *node;

	if (node_p->nodes != NULL) {
		while (node_p->nodes[i] != NULL)
			db_model_node_free((db_tree **)&node_p->nodes[i++]);
		free(node_p->nodes);
	}

	free(*node);
	*node = NULL;
}
