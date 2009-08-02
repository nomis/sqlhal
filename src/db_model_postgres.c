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
	const char *links[] = { "links" };
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

		res = PQexec(conn, "CREATE TABLE nodes (id BIGSERIAL UNIQUE, brain BIGINT NOT NULL, word BIGINT, usage BIGINT NOT NULL, count BIGINT NOT NULL,"\
			" PRIMARY KEY (id),"\
			" FOREIGN KEY (word) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
			" CONSTRAINT valid_usage CHECK (usage >= 0),"\
			" CONSTRAINT valid_count CHECK (count >= 0))");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		nodes_created = 1;
	}
	PQclear(res);

	res = PQexecPrepared(conn, "table_exists", 1, links, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) {
		PQclear(res);

		res = PQexec(conn, "CREATE TABLE links (parent BIGINT NOT NULL, child BIGINT NOT NULL,"\
			" PRIMARY KEY (parent, child),"\
			" FOREIGN KEY (parent) REFERENCES nodes (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
			" FOREIGN KEY (child) REFERENCES nodes (id) ON UPDATE CASCADE ON DELETE CASCADE)");
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
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

	res = PQprepare(conn, "model_fastcreate", "INSERT INTO nodes (brain, word, usage, count) VALUES($1, $2, $3, $4)", 4, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_create_id", "SELECT currval('nodes_id_seq')", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_update", "UPDATE nodes SET word = $2, usage = $3, count = $4 WHERE id = $1", 4, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_link", "INSERT INTO links (parent, child) VALUES($1, $2)", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_root_get", "SELECT forward, backward FROM models WHERE brain = $1", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQprepare(conn, "model_node_get", "SELECT word, usage, count FROM nodes WHERE id = $1", 1, NULL);
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

    res = PQexec(conn, "DEALLOCATE PREPARE model_update");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_link");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_root_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_node_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_root_set");
    PQclear(res);

	return db_hand_free(hand);
}

int db_model_get_order(db_hand **hand, number_t *order) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;

	if (hand == NULL || *hand == NULL || order == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	param[0] = hand_p->brain;
	res = PQexecPrepared(conn, "model_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	if (sizeof(number_t) == sizeof(unsigned long int)) {
		*order = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		*order = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
	} else {
		return -EFAULT;
	}
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

int db_model_set_order(db_hand **hand, number_t order) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp[32];
	number_t tmp2;
	int ret;

	if (hand == NULL || *hand == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	param[0] = hand_p->brain;
	param[1] = tmp;
	if (sizeof(number_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp, "%lu", (unsigned long int)order) <= 0) return -EFAULT;
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp, "%llu", (unsigned long long int)order) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	ret = db_model_get_order(hand, &tmp2);
	if (ret && ret != -ENOTFOUND) return ret;

	if (ret == -ENOTFOUND) {
		res = PQexecPrepared(conn, "model_add", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else {
		res = PQexecPrepared(conn, "model_set", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_set_order", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_zap(db_hand **hand) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;

	if (hand == NULL || *hand == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	param[0] = hand_p->brain;

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
	node->word = 0;
	node->usage = 0;
	node->count = 0;
	node->nodes = NULL;

	return node;
}

int db_model_node_fill(db_hand **hand, db_tree *node) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp0[32];

	if (hand == NULL || *hand == NULL || node == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	param[0] = tmp0;
	if (sizeof(node_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp0, "%lu", (unsigned long int)node->id) <= 0) return -EFAULT;
	} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp0, "%llu", (unsigned long long int)node->id) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	res = PQexecPrepared(conn, "model_node_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	if (sizeof(word_t) == sizeof(unsigned long int)) {
		node->word = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
	} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
		node->word = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
	} else {
		return -EFAULT;
	}

	if (sizeof(number_t) == sizeof(unsigned long int)) {
		node->usage = strtoul(PQgetvalue(res, 0, 1), NULL, 10);
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		node->usage = strtoull(PQgetvalue(res, 0, 1), NULL, 10);
	} else {
		return -EFAULT;
	}

	if (sizeof(number_t) == sizeof(unsigned long int)) {
		node->count = strtoul(PQgetvalue(res, 0, 2), NULL, 10);
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		node->count = strtoull(PQgetvalue(res, 0, 2), NULL, 10);
	} else {
		return -EFAULT;
	}
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

int db_model_get_root(db_hand **hand, db_tree **forward, db_tree **backward) {
	PGresult *res;
	const char *param[3];
	struct db_hand_postgres *hand_p;
	db_tree *forward_p;
	db_tree *backward_p;
	char tmp1[32];
	char tmp2[32];
	int created = 0;
	int ret;

	if (hand == NULL || *hand == NULL || forward == NULL || backward == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	*forward = NULL;
	*backward = NULL;

	param[0] = hand_p->brain;
	res = PQexecPrepared(conn, "model_root_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto fail;

	if (!PQgetisnull(res, 0, 0)) {
		*forward = db_model_node_alloc();
		if (*forward == NULL) return -ENOMEM;
		forward_p = *forward;

		if (sizeof(number_t) == sizeof(unsigned long int)) {
			forward_p->id = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
		} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
			forward_p->id = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
		} else {
			return -EFAULT;
		}

		ret = db_model_node_fill(hand, forward_p);
		if (ret) goto fail;
	} else {
		ret = db_model_create(hand, forward);
		if (ret) goto fail;

		forward_p = *forward;
		created = 1;
	}

	if (!PQgetisnull(res, 0, 1)) {
		*backward = db_model_node_alloc();
		if (*backward == NULL) return -ENOMEM;
		backward_p = *backward;

		if (sizeof(number_t) == sizeof(unsigned long int)) {
			backward_p->id = strtoul(PQgetvalue(res, 0, 1), NULL, 10);
		} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
			backward_p->id = strtoull(PQgetvalue(res, 0, 1), NULL, 10);
		} else {
			return -EFAULT;
		}

		ret = db_model_node_fill(hand, backward_p);
		if (ret) goto fail;
	} else {
		ret = db_model_create(hand, backward);
		if (ret) goto fail;

		backward_p = *backward;
		created = 1;
	}
	PQclear(res);

	if (created) {
		param[1] = tmp1;
		if (sizeof(node_t) == sizeof(unsigned long int)) {
			if (sprintf(tmp1, "%lu", (unsigned long int)forward_p->id) <= 0) return -EFAULT;
		} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
			if (sprintf(tmp1, "%llu", (unsigned long long int)forward_p->id) <= 0) return -EFAULT;
		} else {
			return -EFAULT;
		}

		param[2] = tmp2;
		if (sizeof(node_t) == sizeof(unsigned long int)) {
			if (sprintf(tmp2, "%lu", (unsigned long int)backward_p->id) <= 0) return -EFAULT;
		} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
			if (sprintf(tmp2, "%llu", (unsigned long long int)backward_p->id) <= 0) return -EFAULT;
		} else {
			return -EFAULT;
		}

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

int db_model_create(db_hand **hand, db_tree **node) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	db_tree *node_p;

	if (hand == NULL || *hand == NULL || node == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	*node = db_model_node_alloc();
	if (*node == NULL) return -ENOMEM;
	node_p = *node;

	param[0] = hand_p->brain;

	res = PQexecPrepared(conn, "model_create", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	if (sizeof(word_t) == sizeof(unsigned long int)) {
		node_p->id = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
	} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
		node_p->id = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
	} else {
		return -EFAULT;
	}
	PQclear(res);

	return OK;

fail:
	log_error("db_model_create", PQresultStatus(res), PQresultErrorMessage(res));
	free(*node);
	PQclear(res);
	return -EDB;
}

int db_model_update(db_hand **hand, db_tree *node) {
	PGresult *res;
	const char *param[4];
	struct db_hand_postgres *hand_p;
	char tmp0[32];
	char tmp1[32];
	char tmp2[32];
	char tmp3[32];

	if (hand == NULL || *hand == NULL || node == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	if (node->id == 0) {
		param[0] = hand_p->brain;
	} else {
		param[0] = tmp0;
		if (sizeof(node_t) == sizeof(unsigned long int)) {
			if (sprintf(tmp0, "%lu", (unsigned long int)node->id) <= 0) return -EFAULT;
		} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
			if (sprintf(tmp0, "%llu", (unsigned long long int)node->id) <= 0) return -EFAULT;
		} else {
			return -EFAULT;
		}
	}

	param[1] = tmp1;
	if (sizeof(word_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp1, "%lu", (unsigned long int)node->word) <= 0) return -EFAULT;
	} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp1, "%llu", (unsigned long long int)node->word) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	param[2] = tmp2;
	if (sizeof(number_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp2, "%lu", (unsigned long int)node->usage) <= 0) return -EFAULT;
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp2, "%llu", (unsigned long long int)node->count) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	param[3] = tmp3;
	if (sizeof(number_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp3, "%lu", (unsigned long int)node->usage) <= 0) return -EFAULT;
	} else if (sizeof(number_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp3, "%llu", (unsigned long long int)node->count) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	if (node->id == 0) {
		res = PQexecPrepared(conn, "model_fastcreate", 4, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
		if (PQntuples(res) != 1) goto fail;

		if (sizeof(word_t) == sizeof(unsigned long int)) {
			node->id = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
		} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
			node->id = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
		} else {
			return -EFAULT;
		}
		PQclear(res);
	} else {
		res = PQexecPrepared(conn, "model_update", 4, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_update", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_link(db_hand **hand, db_tree *parent, db_tree *child) {
	PGresult *res;
	const char *param[2];
	struct db_hand_postgres *hand_p;
	char tmp0[32];
	char tmp1[32];

	if (hand == NULL || *hand == NULL || parent == NULL || child == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_model_free(hand);
		return -EDB;
	}

	param[0] = tmp0;
	if (sizeof(node_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp0, "%lu", (unsigned long int)parent->id) <= 0) return -EFAULT;
	} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp0, "%llu", (unsigned long long int)parent->id) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	param[1] = tmp1;
	if (sizeof(node_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp1, "%lu", (unsigned long int)child->id) <= 0) return -EFAULT;
	} else if (sizeof(node_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp1, "%llu", (unsigned long long int)child->id) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

	res = PQexecPrepared(conn, "model_link", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_model_link", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
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
