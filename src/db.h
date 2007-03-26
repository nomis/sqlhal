void db_disconnect();
int db_word_get(const char *word, word_t *ref);
int db_word_add(const char *word, word_t *ref);
int db_list_init(const char *list);
int db_list_add(const char *list, word_t *word);
int db_list_contains(const char *list, word_t *word);
