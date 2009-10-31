enum file_type {
	FILETYPE_MEGAHAL8,
	FILETYPE_SQLHAL0
};

int load_brain(const char *name, const char *filename);
int save_brain(const char *name, enum file_type type, const char *filename);

typedef struct {
	brain_t brain;
	number_t order;
	db_tree **contexts;
} model_t;

typedef struct {
	brain_t brain;
	db_tree *node;
	number_t count;
} model_rand_t;

enum model_dir {
	MODEL_FORWARD = 1,
	MODEL_BACKWARD = 2
};

int model_alloc(brain_t brain, model_t **model);
int model_init(model_t *model, enum model_dir dir);
int model_update(model_t *model, word_t word, int persist);
int model_rand_word(const model_t *model, word_t *word);
int model_rand_init(const model_t *model, model_rand_t *state);
int model_rand_next(model_rand_t *state, word_t *word);
void model_rand_free(model_rand_t *state);
void model_free(model_t **model);
