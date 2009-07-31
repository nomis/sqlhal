#include <libpq-fe.h>

struct db_hand_postgres {
	PGconn *conn;
	char *add;
	char *get;
};

PGconn *conn;

int db_hand_free(db_hand **hand);
