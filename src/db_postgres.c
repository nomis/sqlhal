/* TODO: avoid having to use atol and sprintf with integers... */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "output.h"

struct db_hand_postgres {
	PGconn *conn;
	char *add;
	char *get;
};

PGconn *conn = NULL;

int db_connect() {
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

int db_disconnect() {
	if (conn == NULL)
		return -EDB;

	PQfinish(conn);
	conn = NULL;
	return OK;
}

int db_begin() {
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

int db_commit() {
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

int db_rollback() {
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

int db_word_add(const char *word, word_t *ref) {
	PGresult *res;
	const char *param[1];

	if (word == NULL || ref == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = word;
	res = PQexecPrepared(conn, "word_add", 1, param, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "word_add_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	*ref = atol(PQgetvalue(res, 0, 0));
	PQclear(res);

	return OK;

fail:
	log_error("db_word_add\n", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_word_get(const char *word, word_t *ref) {
	PGresult *res;
	const char *param[1];

	if (word == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = word;
	res = PQexecPrepared(conn, "word_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto end;

	*ref = atol(PQgetvalue(res, 0, 0));
	PQclear(res);

	return OK;

fail:
	log_error("db_word_get", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

end:
	PQclear(res);
	return -ENOTFOUND;
}

int db_list_init(const char *list, db_hand **hand) {
	PGresult *res;
	const char *param[1];
	char *sql;
	struct db_hand_postgres *hand_p;

	if (list == NULL || hand == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	*hand = NULL;
	param[0] = list;
	res = PQexecPrepared(conn, "table_exists", 1, param, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) {
		PQclear(res);

#define SQL "CREATE TABLE %s (word INTEGER NOT NULL, PRIMARY KEY(word), FOREIGN KEY (word) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE)"
		sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
		sprintf(sql, SQL, list);
#undef SQL

		res = PQexec(conn, sql);
		free(sql);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	}
	PQclear(res);

	*hand = malloc(sizeof(struct db_hand_postgres));
	hand_p = *hand;
	hand_p->conn = conn;
	hand_p->add = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(hand_p->add, "list_%s_add", list);
	hand_p->get = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(hand_p->get, "list_%s_get", list);

#define SQL "INSERT INTO %s (word) VALUES($1)"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	sprintf(sql, SQL, list);
#undef SQL

	res = PQprepare(conn, hand_p->add, sql, 1, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

#define SQL "SELECT word FROM %s WHERE word = $1"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	sprintf(sql, SQL, list);
#undef SQL

	res = PQprepare(conn, hand_p->get, sql, 1, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_list_init", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	if (*hand != NULL) {
		free(hand_p->get);
		free(hand_p->add);
		free(*hand);
		*hand = NULL;
	}
	return -EDB;
}

int db_list_free(db_hand **hand) {
	struct db_hand_postgres *hand_p;

	if (hand == NULL || *hand == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) return -EDB;

	if (hand_p->conn != NULL && hand_p->conn == conn) {
		PGresult *res;
		char *sql;

#define SQL "DEALLOCATE PREPARE %s"
		sql = malloc((strlen(SQL) + strlen(hand_p->add)) * sizeof(char));
		sprintf(sql, SQL, hand_p->add);
		res = PQexec(conn, sql);
		free(sql);
		PQclear(res);

		sql = malloc((strlen(SQL) + strlen(hand_p->get)) * sizeof(char));
		sprintf(sql, SQL, hand_p->get);
		res = PQexec(conn, sql);
		free(sql);
		PQclear(res);
#undef SQL
	}

	free(hand_p->add);
	free(hand_p->get);
	free(*hand);
	*hand = NULL;

	return OK;
}

int db_list_add(db_hand **hand, word_t *word) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp[128];

	if (hand == NULL || *hand == NULL || word == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect() || hand_p->conn != conn) {
		hand_p->conn = NULL;
		db_list_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	sprintf(tmp, "%lu", (unsigned long)*word);

	res = PQexecPrepared(conn, hand_p->add, 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_list_add", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_list_contains(db_hand **hand, word_t *word) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp[128];

	if (hand == NULL || *hand == NULL || word == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect() || hand_p->conn != conn) {
		hand_p->conn = NULL;
		db_list_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	sprintf(tmp, "%lu", (unsigned long)*word);

	res = PQexecPrepared(conn, hand_p->get, 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	PQclear(res);

	return OK;

fail:
	log_error("db_list_contains", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_map_init(const char *map, db_hand **hand) {
	PGresult *res;
	const char *param[1];
	char *sql;
	struct db_hand_postgres *hand_p;

	if (map == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	*hand = NULL;
	param[0] = map;
	res = PQexecPrepared(conn, "table_exists", 1, param, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) {
		PQclear(res);

#define SQL "CREATE TABLE %s (key INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY(key), FOREIGN KEY (key) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE, FOREIGN KEY (value) REFERENCES words (id) ON UPDATE CASCADE ON DELETE CASCADE)"
		sql = malloc((strlen(SQL) + strlen(map)) * sizeof(char));
		sprintf(sql, SQL, map);
#undef SQL

		res = PQexec(conn, sql);
		free(sql);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	}
	PQclear(res);

	*hand = malloc(sizeof(struct db_hand_postgres));
	hand_p = *hand;
	hand_p->conn = conn;
	hand_p->add = malloc((9 + strlen(map)) * sizeof(char));
	sprintf(hand_p->add, "map_%s_add", map);
	hand_p->get = malloc((9 + strlen(map)) * sizeof(char));
	sprintf(hand_p->get, "map_%s_get", map);

#define SQL "INSERT INTO %s (key, value) VALUES($1, $2)"
	sql = malloc((strlen(SQL) + strlen(map)) * sizeof(char));
	sprintf(sql, SQL, map);
#undef SQL

	res = PQprepare(conn, hand_p->add, sql, 2, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

#define SQL "SELECT value FROM %s WHERE key = $1"
	sql = malloc((strlen(SQL) + strlen(map)) * sizeof(char));
	sprintf(sql, SQL, map);
#undef SQL

	res = PQprepare(conn, hand_p->get, sql, 1, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_map_init", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	if (*hand != NULL) {
		free(hand_p->get);
		free(hand_p->add);
		free(*hand);
		*hand = NULL;
	}
	return -EDB;
}

int db_map_free(db_hand **hand) {
	struct db_hand_postgres *hand_p;

	if (hand == NULL || *hand == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) return -EDB;

	if (hand_p->conn != NULL && hand_p->conn == conn) {
		PGresult *res;
		char *sql;

#define SQL "DEALLOCATE PREPARE %s"
		sql = malloc((strlen(SQL) + strlen(hand_p->add)) * sizeof(char));
		sprintf(sql, SQL, hand_p->add);
		res = PQexec(conn, sql);
		free(sql);
		PQclear(res);

		sql = malloc((strlen(SQL) + strlen(hand_p->get)) * sizeof(char));
		sprintf(sql, SQL, hand_p->get);
		res = PQexec(conn, sql);
		free(sql);
		PQclear(res);
#undef SQL
	}

	free(hand_p->add);
	free(hand_p->get);
	free(*hand);
	*hand = NULL;

	return OK;
}

int db_map_add(db_hand **hand, word_t *key, word_t *value) {
	PGresult *res;
	const char *param[2];
	struct db_hand_postgres *hand_p;
	char tmp[128];
	char tmp2[128];

	if (hand == NULL || *hand == NULL || key == NULL || value == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect() || hand_p->conn != conn) {
		hand_p->conn = NULL;
		db_map_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	param[1] = tmp2;
	sprintf(tmp, "%lu", (unsigned long)*key);
	sprintf(tmp2, "%lu", (unsigned long)*value);

	res = PQexecPrepared(conn, hand_p->add, 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_map_add", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_map_get(db_hand **hand, word_t *key, word_t *value) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp[128];

	if (hand == NULL || *hand == NULL || key == NULL || value == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect() || hand_p->conn != conn) {
		hand_p->conn = NULL;
		db_map_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	sprintf(tmp, "%lu", (unsigned long)*key);

	res = PQexecPrepared(conn, hand_p->get, 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	*value = atol(PQgetvalue(res, 0, 0));

	PQclear(res);

	return OK;

fail:
	log_error("db_map_contains", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}
