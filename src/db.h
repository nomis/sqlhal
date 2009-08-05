int db_connect(void);         /* creates tables, prepares statements */
int db_disconnect(void);      /* deallocates prepared statements */

int db_begin(void);
int db_commit(void);
int db_rollback(void);

int db_brain_add(const char *brain, brain_t *ref);                           /* add brain (does not exist) */
int db_brain_get(const char *brain, brain_t *ref);                           /* return -ENOTFOUND if brain does not exist */
int db_brain_use(const char *brain, brain_t *ref);                           /* get or add brain */

int db_word_add(const char *word, word_t *ref);                              /* add word (does not exist) */
int db_word_get(const char *word, word_t *ref);                              /* return -ENOTFOUND if word does not exist */
int db_word_use(const char *word, word_t *ref);                              /* get or add word */

int db_list_zap(brain_t brain, enum list type);                              /* clears table */
int db_list_add(brain_t brain, enum list type, word_t word);                 /* add word (does not exist) */
int db_list_contains(brain_t brain, enum list type, word_t word);            /* return -ENOTFOUND if word does not exist */
int db_list_del(brain_t brain, enum list type, word_t word);                 /* delete word (exists) */

int db_map_zap(brain_t brain, enum map type);                                /* clears table */
int db_map_get(brain_t brain, enum map type, word_t key, word_t *value);     /* return -ENOTFOUND if key does not exist */
int db_map_put(brain_t brain, enum map type, word_t key, word_t value);      /* add key (does not exist) */
int db_map_del(brain_t brain, enum map type, word_t key);                    /* delete key (exists) */

typedef struct {
	node_t id;
	node_t parent_id;
	word_t word;
	number_t usage;
	number_t count;

	number_t children;
	void **nodes;
} db_tree;

int db_model_zap(brain_t brain);                                             /* clears table */
int db_model_get_order(brain_t brain, number_t *order);                      /* get model order (may return -ENOTFOUND) */
int db_model_set_order(brain_t brain, number_t order);                       /* set model order (required) */

int db_model_get_root(brain_t brain, db_tree **forward, db_tree **backward); /* get or create forward/backward nodes */
db_tree *db_model_node_alloc(void);                                          /* allocate node for creation on first update */
int db_model_create(brain_t brain, db_tree **node);                          /* create node */
int db_model_update(brain_t brain, db_tree *node);                           /* update node */
int db_model_link(db_tree *parent, db_tree *child);                          /* add node to tree */
int db_model_node_fill(brain_t brain, db_tree *node);                        /* load children */
void db_model_node_free(db_tree **node);                                     /* free node data (recursively) */

int db_model_dump_words(brain_t brain, uint_fast32_t *dict_size, word_t **dict_words, char ***dict_text);
