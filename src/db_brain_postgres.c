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

int db_brain_add(const char *brain, brain_t *ref) {
	PGresult *res;
	const char *param[1];

	if (brain == NULL || ref == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = brain;
	res = PQexecPrepared(conn, "brain_add", 1, param, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "brain_add_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	if (sizeof(brain_t) == sizeof(unsigned long int)) {
		*ref = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
	} else if (sizeof(brain_t) == sizeof(unsigned long long int)) {
		*ref = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
	} else {
		return -EFAULT;
	}
	PQclear(res);

	return OK;

fail:
	log_error("db_brain_add", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_brain_get(const char *brain, brain_t *ref) {
	PGresult *res;
	const char *param[1];

	if (brain == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = brain;
	res = PQexecPrepared(conn, "brain_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto end;

	if (sizeof(brain_t) == sizeof(unsigned long int)) {
		*ref = strtoul(PQgetvalue(res, 0, 0), NULL, 10);
	} else if (sizeof(brain_t) == sizeof(unsigned long long int)) {
		*ref = strtoull(PQgetvalue(res, 0, 0), NULL, 10);
	} else {
		return -EFAULT;
	}
	PQclear(res);

	return OK;

fail:
	log_error("db_brain_get", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

end:
	PQclear(res);
	return -ENOTFOUND;
}
