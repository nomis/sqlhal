int load_brain(const char *name, const char *filename);
int save_brain(const char *name, const char *filename);

typedef struct {
	number_t order;
	db_tree *forward;
	db_tree *backward;
	db_tree **contexts;
} model_t;

int model_alloc(brain_t brain, model_t **model);
void model_free(model_t **model);
