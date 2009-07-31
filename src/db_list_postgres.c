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

int db_list_init(const char *list, db_hand **hand) {
	PGresult *res;
	const char *param[1];
	char *sql;
	struct db_hand_postgres *hand_p;
	int ret;

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
		if (sql == NULL) return -ENOMEM;
		if (sprintf(sql, SQL, list) <= 0) { free(sql); return -EFAULT; }
#undef SQL

		res = PQexec(conn, sql);
		free(sql);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	}
	PQclear(res);

	ret = db_hand_init(hand);
	if (ret) return ret;
	hand_p = *hand;

	hand_p->add = malloc((10 + strlen(list)) * sizeof(char));
	if (hand_p->add == NULL) { ret = -ENOMEM; goto fail_free; }
	if (sprintf(hand_p->add, "list_%s_add", list) <= 0) { ret = -ENOMEM; goto fail_free; }
	hand_p->get = malloc((10 + strlen(list)) * sizeof(char));
	if (hand_p->get == NULL) { ret = -ENOMEM; goto fail_free; }
	if (sprintf(hand_p->get, "list_%s_get", list) <= 0) { ret = -ENOMEM; goto fail_free; }

#define SQL "INSERT INTO %s (word) VALUES($1)"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	if (sql == NULL) { ret = -ENOMEM; goto fail_free; }
	if (sprintf(sql, SQL, list) <= 0) { ret = -EFAULT; free(sql); goto fail_free; }
#undef SQL

	res = PQprepare(conn, hand_p->add, sql, 1, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

#define SQL "SELECT word FROM %s WHERE word = $1"
	sql = malloc((strlen(SQL) + strlen(list)) * sizeof(char));
	if (sql == NULL) { ret = -ENOMEM; goto fail_free; }
	if (sprintf(sql, SQL, list) <= 0) { ret = -EFAULT; free(sql); goto fail_free; }
#undef SQL

	res = PQprepare(conn, hand_p->get, sql, 1, NULL);
	free(sql);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_list_init", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	ret = -EDB;
fail_free:
	if (*hand != NULL) {
		if (hand_p->get != NULL) free(hand_p->get);
		if (hand_p->add != NULL) free(hand_p->add);
		free(*hand);
		*hand = NULL;
	}
	return ret;
}

int db_list_free(db_hand **hand) {
	return db_hand_free(hand);
}

int db_list_add(db_hand **hand, word_t *word) {
	PGresult *res;
	const char *param[1];
	struct db_hand_postgres *hand_p;
	char tmp[128];

	if (hand == NULL || *hand == NULL || word == NULL) return -EINVAL;
	hand_p = *hand;
	if (db_connect()) {
		db_list_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	if (sizeof(word_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp, "%lu", (unsigned long int)*word) <= 0) return -EFAULT;
	} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp, "%llu", (unsigned long long int)*word) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

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
	if (db_connect()) {
		db_list_free(hand);
		return -EDB;
	}

	param[0] = tmp;
	if (sizeof(word_t) == sizeof(unsigned long int)) {
		if (sprintf(tmp, "%lu", (unsigned long int)*word) <= 0) return -EFAULT;
	} else if (sizeof(word_t) == sizeof(unsigned long long int)) {
		if (sprintf(tmp, "%llu", (unsigned long long int)*word) <= 0) return -EFAULT;
	} else {
		return -EFAULT;
	}

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
