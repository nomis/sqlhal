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
			const char *words[] = { "words" };

			res = PQprepare(conn, "table_exists", "SELECT tablename FROM pg_tables WHERE schemaname = 'public' AND tablename = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQexecPrepared(conn, "table_exists", 1, words, NULL, NULL, 1);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);
				res = PQexec(conn, "CREATE TABLE words (id SERIAL UNIQUE, word TEXT, added TIMESTAMP NOT NULL DEFAULT NOW(), PRIMARY KEY (word))");
				if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			}
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
	if (conn == NULL)
		return -EDB;

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
#undef SQL
	}

	free(hand_p->add);
	free(hand_p->get);
	free(*hand);
	*hand = NULL;

	return ret;
}
