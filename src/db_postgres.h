#include <libpq-fe.h>

PGconn *conn;

#define SET_PARAM(param, buf, pos, value) do { \
	param[pos] = buf[pos]; \
	if (sizeof(value) == sizeof(unsigned int)) { \
		if (sprintf(buf[pos], "%u", (unsigned int)(value)) <= 0) return -EFAULT; \
	} else if (sizeof(value) == sizeof(unsigned long int)) { \
		if (sprintf(buf[pos], "%lu", (unsigned long int)(value)) <= 0) return -EFAULT; \
	} else if (sizeof(value) == sizeof(unsigned long long int)) { \
		if (sprintf(buf[pos], "%llu", (unsigned long long int)(value)) <= 0) return -EFAULT; \
	} else { \
		log_fatal("SET_PARAM", sizeof(value), "Unhandled numeric sizeof"); \
		return -EFAULT; \
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
		return -EFAULT; \
	} \
} while(0)

