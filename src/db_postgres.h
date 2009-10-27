#include <libpq-fe.h>

PGconn *conn;

#define SET_PARAM(param, buf, pos, value) do { \
	param[pos] = buf[pos]; \
	if (sizeof(value) == sizeof(unsigned int)) { \
		BUG_IF(sprintf(buf[pos], "%u", (unsigned int)(value)) <= 0); \
	} else if (sizeof(value) == sizeof(unsigned long int)) { \
		BUG_IF(sprintf(buf[pos], "%lu", (unsigned long int)(value)) <= 0); \
	} else if (sizeof(value) == sizeof(unsigned long long int)) { \
		BUG_IF(sprintf(buf[pos], "%llu", (unsigned long long int)(value)) <= 0); \
	} else { \
		log_fatal("SET_PARAM", sizeof(value), "Unhandled numeric sizeof"); \
		BUG(); \
	} \
} while(0)

#define GET_VALUE(res, tup_num, field_num, lvalue) do { \
	if (sizeof(lvalue) == sizeof(unsigned int)) { \
		lvalue = strtoul(PQgetvalue(res, tup_num, field_num), NULL, 10); \
	} else if (sizeof(lvalue) == sizeof(unsigned long int)) { \
		lvalue = strtoul(PQgetvalue(res, tup_num, field_num), NULL, 10); \
	} else if (sizeof(lvalue) == sizeof(unsigned long long int)) { \
		lvalue = strtoull(PQgetvalue(res, tup_num, field_num), NULL, 10); \
	} else { \
		log_fatal("SET_PARAM", sizeof(lvalue), "Unhandled numeric sizeof"); \
		PQclear(res); \
		BUG(); \
	} \
} while(0)

