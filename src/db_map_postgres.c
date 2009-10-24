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

int db_map_put(brain_t brain, enum map type, word_t key, word_t value) {
	PGresult *res;
	const char *param[4];
	char tmp[4][32];

	if (brain == 0 || key == 0 || value == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);
	SET_PARAM(param, tmp, 2, key);
	SET_PARAM(param, tmp, 3, value);

	res = PQexecPrepared(conn, "map_add", 4, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_map_put", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_map_get(brain_t brain, enum map type, word_t key, word_t *value) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];

	if (brain == 0 || type == 0 || key == 0 || value == NULL) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);
	SET_PARAM(param, tmp, 2, key);

	res = PQexecPrepared(conn, "map_get", 3, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	GET_VALUE(res, 0, 0, *value);

	PQclear(res);

	return OK;

fail:
	log_error("db_map_get", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_map_iter(brain_t brain, enum list type, int (*callback)(void *data, word_t word_ref, word_t value_ref, const char *key, const char *value), void *data) {
	PGresult *res;
	unsigned int num, i;
	const char *param[2];
	char tmp[2][32];

	if (brain == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);

	res = PQexecPrepared(conn, "map_iter", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	num = PQntuples(res);

	for (i = 0; i < num; i++) {
		word_t key_ref, value_ref;
		char *key, *value;
		int ret;

		GET_VALUE(res, i, 0, key_ref);
		GET_VALUE(res, i, 1, value_ref);
		key = PQgetvalue(res, i, 2);
		value = PQgetvalue(res, i, 3);

		ret = callback(data, key_ref, value_ref, key, value);
		if (ret) goto fail;
	}

	PQclear(res);

	return OK;

fail:
	log_error("db_map_iter", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_map_zap(brain_t brain, enum map type) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];

	if (brain == 0) return -EINVAL;
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, type);

	res = PQexecPrepared(conn, "map_zap", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_map_zap", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}
