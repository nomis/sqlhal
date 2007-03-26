#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "err.h"
#include "types.h"
#include "db.h"

static PGconn *conn = NULL;

int db_connect() {
	if (conn == NULL) {
		conn = PQconnectdb("");

		if (conn == NULL)
			return -EDB;

		if (PQstatus(conn) != CONNECTION_OK) {
			printf("DB: %d %s\n", PQstatus(conn), PQerrorMessage(conn));
			PQfinish(conn);
			conn = NULL;
		} else {
			PGresult *res = NULL;
			const char *words[] = { "words" };

			res = PQprepare(conn, "table_exists", "SELECT tablename FROM pg_tables WHERE schemaname = 'public' AND tablename = $1", 1, NULL);
			if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
			PQclear(res);

			res = PQexecPrepared(conn, "table_exists", 1, words, NULL, NULL, 0);
			if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
			if (PQntuples(res) != 1) {
				PQclear(res);
				res = PQexec(conn, "CREATE TABLE words (id SERIAL UNIQUE, word TEXT, PRIMARY KEY (word))");
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
				printf("db_connect: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
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

void db_disconnect() {
	if (conn != NULL) {
		PQfinish(conn);
		conn = NULL;
	}
}

int db_word_add(const char *word, word_t *ref) {
	PGresult *res;
	const char *param[1];

	if (word == NULL || ref == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = word;
	res = PQexecPrepared(conn, "word_add", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "word_add_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	*ref = atol(PQgetvalue(res, 0, 0));
	PQclear(res);

	return OK;

fail:
	printf("db_word_add: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
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
	printf("db_word_get: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

end:
	PQclear(res);
	return -ENOTFOUND;
}

int db_list_init(const char *list) {
	PGresult *res;
	const char *param[1];
	char *sql, *name;

	if (list == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = list;
	res = PQexecPrepared(conn, "table_exists", 1, param, NULL, NULL, 0);
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

	name = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(name, "list_%s_add", list);

#define SQL "INSERT INTO %s (word) VALUES($1)"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	sprintf(sql, SQL, list);
#undef SQL

	res = PQprepare(conn, name, sql, 1, NULL);
	free(name);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	name = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(name, "list_%s_get", list);

#define SQL "SELECT word FROM %s WHERE word = $1"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	sprintf(sql, SQL, list);
#undef SQL

	res = PQprepare(conn, name, sql, 1, NULL);
	free(name);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	printf("db_list_init: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_list_add(const char *list, word_t *word) {
	PGresult *res;
	const char *param[1];
	char *name;
	static char tmp[128];

	if (list == NULL || word == NULL) return -EINVAL;

	name = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(name, "list_%s_add", list);

	param[0] = tmp;
	sprintf(tmp, "%lu", (unsigned long)*word);

	res = PQexecPrepared(conn, name, 1, param, NULL, NULL, 0);
	free(name);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	printf("db_list_add: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_list_contains(const char *list, word_t *word) {
	PGresult *res;
	const char *param[1];
	char *name;
	static char tmp[128];

	if (list == NULL || word == NULL) return -EINVAL;

	name = malloc((10 + strlen(list)) * sizeof(char));
	sprintf(name, "list_%s_get", list);

	param[0] = tmp;
	sprintf(tmp, "%lu", (unsigned long)*word);

	res = PQexecPrepared(conn, name, 1, param, NULL, NULL, 0);
	free(name);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	PQclear(res);

	return OK;

fail:
	printf("db_list_add: %d %s\n", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

