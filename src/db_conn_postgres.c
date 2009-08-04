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

PGconn *conn = NULL;

int db_connect(void) {
	if (conn == NULL) {
		conn = PQconnectdb("");

		if (conn == NULL)
			return -EDB;

		if (PQstatus(conn) != CONNECTION_OK) {
			log_error("DB", PQstatus(conn), PQerrorMessage(conn));
			PQfinish(conn);
			conn = NULL;
		} else {
			PGresult *res = NULL;
			const char *brains[] = { "brains" };
			const char *words[] = { "words" };
			const char *models[] = { "models" };
			const char *nodes[] = { "nodes" };
			int nodes_created = 0;

			res = PQprepare(conn, "table_exists", "SELECT tablename FROM pg_tables WHERE schemaname = 'public' AND tablename = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQexecPrepared(conn, "table_exists", 1, brains, NULL, NULL, 1);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);
				res = PQexec(conn, "CREATE TABLE brains (id BIGSERIAL UNIQUE, name TEXT,"\
					" PRIMARY KEY (name),"\
					" CONSTRAINT valid_id CHECK (id > 0))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			}
			PQclear(res);

			res = PQexecPrepared(conn, "table_exists", 1, words, NULL, NULL, 1);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);
				res = PQexec(conn, "CREATE TABLE words (id SERIAL UNIQUE, word TEXT, added TIMESTAMP NOT NULL DEFAULT NOW(),"\
					" PRIMARY KEY (word),"\
					" CONSTRAINT valid_id CHECK (id > 0))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			}
			PQclear(res);

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

			res = PQprepare(conn, "brain_add", "INSERT INTO brains (name) VALUES($1)", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "brain_add_id", "SELECT currval('brains_id_seq')", 0, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "brain_get", "SELECT id FROM brains WHERE name = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "word_add", "INSERT INTO words (word) VALUES($1)", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "word_add_id", "SELECT currval('words_id_seq')", 0, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "word_get", "SELECT id FROM words WHERE word = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

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

			res = NULL;

fail:
			if (res != NULL) {
				log_error("db_connect", PQresultStatus(res), PQresultErrorMessage(res));
				PQclear(res);
				PQfinish(conn);
				conn = NULL;
			}
		}
	}

	if (conn == NULL)
		return -EDB;

	return OK;
}

int db_disconnect(void) {
	PGresult *res;

	if (conn == NULL)
		return -EDB;

	res = PQexec(conn, "DEALLOCATE PREPARE brain_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE brain_add_id");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE brain_get");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE word_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE word_add_id");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE word_get");
	PQclear(res);

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

	PQfinish(conn);
	conn = NULL;
	return OK;
}

int db_begin(void) {
	PGresult *res;

	if (db_connect()) return -EDB;

	res = PQexec(conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	PQclear(res);
	return -EDB;
}

int db_commit(void) {
	PGresult *res;

	if (db_connect()) return -EDB;

	res = PQexec(conn, "COMMIT");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	PQclear(res);
	return -EDB;
}

int db_rollback(void) {
	PGresult *res;

	if (db_connect()) return -EDB;

	res = PQexec(conn, "ROLLBACK");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	PQclear(res);
	return -EDB;
}

int db_hand_init(db_hand **hand) {
	struct db_hand_postgres *hand_p;
	int ret = OK;

	if (hand == NULL || *hand != NULL) return -EINVAL;

    *hand = malloc(sizeof(struct db_hand_postgres));
    if (*hand == NULL) return -ENOMEM;
    hand_p = *hand;

	hand_p->brain = NULL;
	hand_p->get = NULL;
	hand_p->add = NULL;

	return ret;
}

int db_hand_free(db_hand **hand) {
	struct db_hand_postgres *hand_p;
	int ret = OK;

	if (hand == NULL || *hand == NULL) return -EINVAL;
	hand_p = *hand;

	{
		PGresult *res;
		char *sql;

		ret = db_connect();

#define SQL "DEALLOCATE PREPARE %s"
		if (hand_p->add != NULL) {
			if (!ret) {
				sql = malloc((strlen(SQL) + strlen(hand_p->add)) * sizeof(char));
				if (sql == NULL) ret = -ENOMEM;
				else if (sprintf(sql, SQL, hand_p->add) <= 0) { ret = -EFAULT; free(sql); }
			}

			if (!ret) {
				res = PQexec(conn, sql);
				free(sql);
				PQclear(res);
			}
		}

		if (hand_p->get != NULL) {
			if (!ret) {
				sql = malloc((strlen(SQL) + strlen(hand_p->get)) * sizeof(char));
				if (sql == NULL) ret = -ENOMEM;
				else if (sprintf(sql, SQL, hand_p->get) <= 0) { ret = -EFAULT; free(sql); }
			}

			if (!ret) {
				res = PQexec(conn, sql);
				free(sql);
				PQclear(res);
			}
		}
#undef SQL
	}

	free(hand_p->brain);
	free(hand_p->add);
	free(hand_p->get);
	free(*hand);
	*hand = NULL;

	return ret;
}
