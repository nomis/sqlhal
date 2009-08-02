#include <libpq-fe.h>

struct db_hand_postgres {
	char *brain;
	char *add;
	char *get;
	char *zap;
};

PGconn *conn;

int db_hand_init(db_hand **hand);
int db_hand_free(db_hand **hand);
