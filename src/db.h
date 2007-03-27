typedef void db_hand;

int db_connect();
void db_disconnect();

int db_begin();
int db_commit();
int db_rollback();

int db_word_add(const char *word, word_t *ref);
int db_word_get(const char *word, word_t *ref);

int db_list_init(const char *list, db_hand **hand);
int db_list_free(db_hand **hand);
int db_list_add(db_hand **hand, word_t *word);
int db_list_contains(db_hand **hand, word_t *word);

int db_map_init(const char *map, db_hand **hand);
int db_map_free(db_hand **hand);
int db_map_add(db_hand **hand, word_t *key, word_t *value);
int db_map_get(db_hand **hand, word_t *key, word_t *value);
