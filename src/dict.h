int load_list(const char *name, enum list type, const char *filename);
int save_list(const char *name, enum list type, const char *filename);

int load_map(const char *name, enum map type, const char *filename);
int save_map(const char *name, enum map type, const char *filename);

dict_t *dict_alloc(void);
int dict_add(dict_t *dict, word_t word, uint32_t *pos);
int dict_del(dict_t *dict, word_t word, uint32_t *pos);
int dict_size(dict_t *dict, uint32_t *size);
int dict_find(dict_t *dict, word_t word, uint32_t *pos);
void dict_free(dict_t **dict);

list_t *list_alloc(void);
int list_append(list_t *list, word_t word);
int list_prepend(list_t *list, word_t word);
int list_get(list_t *list, uint32_t pos, word_t *word);
int list_set(list_t *list, uint32_t pos, word_t word);
int list_size(list_t *list, uint32_t *size);
void list_free(list_t **list);
