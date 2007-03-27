typedef void db_list;

int db_connect();
void db_disconnect();

int db_begin();
int db_commit();
int db_rollback();

int db_word_add(const char *word, word_t *ref);
int db_word_get(const char *word, word_t *ref);

int db_list_init(const char *list, db_list **hand);
int db_list_free(db_list **hand);
int db_list_add(db_list **hand, word_t *word);
int db_list_contains(db_list **hand, word_t *word);
