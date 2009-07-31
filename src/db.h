typedef void db_hand;

int db_connect(void);    /* creates words table */
int db_disconnect(void);

int db_begin(void);
int db_commit(void);
int db_rollback(void);

int db_word_add(const char *word, word_t *ref); /* add word (does not exist) */
int db_word_get(const char *word, word_t *ref); /* return -ENOTFOUND if word does not exist */
int db_word_use(const char *word, word_t *ref); /* get or add word */

int db_list_init(const char *list, db_hand **hand); /* create table, prepare statements */
int db_list_free(db_hand **hand);                   /* deallocates prepared statements */
int db_list_zap(db_hand **hand);                    /* drops table */
int db_list_add(db_hand **hand, word_t *word);      /* add word (does not exist) */
int db_list_contains(db_hand **hand, word_t *word); /* return -ENOTFOUND if word does not exist */
int db_list_del(db_hand **hand, word_t *word);      /* delete word (exists) */

int db_map_init(const char *map, db_hand **hand);           /* create table, prepare statements */
int db_map_free(db_hand **hand);                            /* deallocates prepared statements */
int db_map_zap(db_hand **hand);                             /* drops table */
int db_map_get(db_hand **hand, word_t *key, word_t *value); /* return -ENOTFOUND if key does not exist */
int db_map_put(db_hand **hand, word_t *key, word_t *value); /* add key (does not exist) */
int db_map_use(db_hand **hand, word_t *key, word_t *value); /* get or put word */
int db_map_del(db_hand **hand, word_t *key, word_t *value); /* delete key (exists) */
