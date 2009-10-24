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

int db_list_add(brain_t brain, enum list type, word_t word) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];

	if (brain == 0 || word == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);
	SET_PARAM(param, tmp, 2, word);

	res = PQexecPrepared(conn, "list_add", 3, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_list_add", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_list_contains(brain_t brain, enum list type, word_t word) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];

	if (brain == 0 || word == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);
	SET_PARAM(param, tmp, 2, word);

	res = PQexecPrepared(conn, "list_get", 3, param, NULL, NULL, 0);
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

int db_list_iter(brain_t brain, enum list type, int (*callback)(void *data, word_t ref, const char *word), void *data) {
	PGresult *res;
	unsigned int num, i;
	const char *param[2];
	char tmp[2][32];

	if (brain == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);

	res = PQexecPrepared(conn, "list_iter", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	num = PQntuples(res);

	for (i = 0; i < num; i++) {
		word_t ref;
		char *word;
		int ret;

		GET_VALUE(res, i, 0, ref);
		word = PQgetvalue(res, i, 1);

		ret = callback(data, ref, word);
		if (ret) goto fail;
	}

	PQclear(res);

	return OK;

fail:
	log_error("db_list_iter", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_list_zap(brain_t brain, enum list type) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];

	if (brain == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);

	res = PQexecPrepared(conn, "list_zap", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_list_zap", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}
