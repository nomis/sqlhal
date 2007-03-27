typedef struct {
	word_t word;
	number_t count;
	struct db_tree *nodes[0];
} db_tree;

int db_model_init(const char *name, db_hand **hand);
int db_model_free(db_hand **hand);
int db_model_zap(db_hand **hand);

int db_model_get_order(db_hand **hand, number_t *order);
int db_model_set_order(db_hand **hand, number_t order);
int db_model_get_forward(db_hand **hand, db_tree **tree);
int db_model_get_backward(db_hand **hand, db_tree **tree);

int db_tree_add_branch(db_hand **hand, db_tree **tree);
int db_tree_get_branch(db_hand **hand, db_tree **tree, number_t position);
int db_tree_ins_branch(db_hand **hand, db_tree **tree, number_t position);
int db_tree_del_branch(db_hand **hand, db_tree **tree, number_t position);
int db_tree_free(db_tree **tree);
