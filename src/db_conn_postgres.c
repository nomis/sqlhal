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
			const char *lists[] = { "lists" };
			const char *maps[] = { "maps" };
			const char *models[] = { "models" };
			const char *nodes[] = { "nodes" };
			int nodes_created = 0;
			int server_ver;

			server_ver = PQserverVersion(conn);
			if (server_ver < 80400) {
				log_error("DB", server_ver, "Server version must be 8.4.0+");
				PQfinish(conn);
				conn = NULL;
				return -EDB;
			}

			res = PQprepare(conn, "table_exists", "SELECT tablename FROM pg_tables WHERE schemaname = 'public' AND tablename = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			/* BRAIN */

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

			/* WORD */

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

			/* LIST */

			res = PQexecPrepared(conn, "table_exists", 1, lists, NULL, NULL, 1);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);

				res = PQexec(conn, "CREATE TABLE lists (type INT NOT NULL, brain BIGINT NOT NULL, word BIGINT NOT NULL,"\
					" PRIMARY KEY (brain, type, word),"\
					" FOREIGN KEY (brain) REFERENCES brains (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
					" FOREIGN KEY (word) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
					" CONSTRAINT valid_type CHECK (type >= 1 AND type <= 3))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
				PQclear(res);

				res = PQexec(conn, "CREATE INDEX lists_words ON lists (word)");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			}
			PQclear(res);

			/* MAP */

			res = PQexecPrepared(conn, "table_exists", 1, maps, NULL, NULL, 1);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);

				res = PQexec(conn, "CREATE TABLE maps (type INT NOT NULL, brain BIGINT NOT NULL, key BIGINT NOT NULL, value BIGINT NOT NULL,"\
					" PRIMARY KEY (brain, key),"\
					" FOREIGN KEY (brain) REFERENCES brains (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
					" FOREIGN KEY (key) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
					" FOREIGN KEY (value) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE,"\
					" CONSTRAINT valid_type CHECK (type = 4))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
				PQclear(res);

				res = PQexec(conn, "CREATE INDEX maps_keys ON maps (key)");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
				PQclear(res);

				res = PQexec(conn, "CREATE INDEX maps_values ON maps (value)");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			}
			PQclear(res);

			/* MODEL */

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
					" CONSTRAINT valid_count CHECK (count >= 0),"\
					" CONSTRAINT valid_root CHECK (parent IS NOT NULL OR word IS NULL),"\
					" CONSTRAINT valid_fin CHECK (parent IS NULL OR word IS NOT NULL OR usage = 0))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
				PQclear(res);

				res = PQexec(conn, "CREATE INDEX nodes_words ON nodes (word)");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
				PQclear(res);

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

			/* BRAIN */

			res = PQprepare(conn, "brain_add", "INSERT INTO brains (name) VALUES($1)", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "brain_add_id", "SELECT currval('brains_id_seq')", 0, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "brain_get", "SELECT id FROM brains WHERE name = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			/* WORD */

			res = PQprepare(conn, "word_add", "INSERT INTO words (word) VALUES($1)", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "word_add_id", "SELECT currval('words_id_seq')", 0, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "word_get", "SELECT id FROM words WHERE word = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			/* LIST */

			res = PQprepare(conn, "list_add", "INSERT INTO lists (brain, type, word) VALUES($1, $2, $3)", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "list_get", "SELECT word FROM lists WHERE brain = $1 AND type = $2 AND word = $3", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "list_zap", "DELETE FROM lists WHERE brain = $1 AND type = $2", 2, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			/* MAP */

			res = PQprepare(conn, "map_add", "INSERT INTO maps (brain, type, key, value) VALUES($1, $2, $3, $4)", 4, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "map_get", "SELECT value FROM maps WHERE brain = $1 AND type = $2 AND key = $3", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "map_zap", "DELETE FROM maps WHERE brain = $1 AND type = $2", 2, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			/* MODEL */

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

			res = PQprepare(conn, "model_rootupdate", "UPDATE nodes SET parent = NULL, usage = $2, count = $3 WHERE id = $1", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "model_update", "UPDATE nodes SET usage = $2, count = $3 WHERE id = $1", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "model_root_get", "SELECT forward, backward FROM models WHERE brain = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "model_root_set", "UPDATE models SET forward = $2, backward = $3 WHERE brain = $1", 3, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "model_node_get", "SELECT id, word, usage, count FROM nodes"\
				" WHERE brain = $1 AND (id = $2 OR parent = $2)"\
				" ORDER BY (SELECT words.word FROM words WHERE words.id = nodes.word) NULLS LAST", 2, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQprepare(conn, "model_brain_words", "SELECT id, ROW_NUMBER() OVER (ORDER BY id) - 1, word "\
				" FROM words WHERE id IN (SELECT word FROM nodes WHERE brain=$1) ORDER BY word", 1, NULL);
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

	/* BRAIN */

	res = PQexec(conn, "DEALLOCATE PREPARE brain_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE brain_add_id");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE brain_get");
	PQclear(res);

	/* WORD */

	res = PQexec(conn, "DEALLOCATE PREPARE word_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE word_add_id");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE word_get");
	PQclear(res);

	/* LIST */

	res = PQexec(conn, "DEALLOCATE PREPARE list_get");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE list_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE list_zap");
	PQclear(res);

	/* MAP */

	res = PQexec(conn, "DEALLOCATE PREPARE map_get");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE map_add");
	PQclear(res);

	res = PQexec(conn, "DEALLOCATE PREPARE map_zap");
	PQclear(res);

	/* MODEL */

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

    res = PQexec(conn, "DEALLOCATE PREPARE model_root_set");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_node_get");
    PQclear(res);

    res = PQexec(conn, "DEALLOCATE PREPARE model_brain_words");
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
