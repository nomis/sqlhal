#include <stdint.h>
#include <stdlib.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "megahal.h"

int db_brain_use(const char *brain, brain_t *ref) {
	int ret;

	ret = db_brain_get(brain, ref);
	if (ret == -ENOTFOUND) {
		ret = db_brain_add(brain, ref);
		if (ret) return ret;

		ret = db_model_set_order(*ref, MEGAHAL_DEFAULT_ORDER);
	}
	return ret;
}

int db_word_use(const char *word, word_t *ref) {
	int ret;

	WARN_IF(word == NULL);
	WARN_IF(word[0] == 0);

	ret = db_word_get(word, ref);
	if (ret == -ENOTFOUND)
		ret = db_word_add(word, ref);
	return ret;
}

db_tree *db_model_node_alloc(void) {
	db_tree *node;

	node = malloc(sizeof(db_tree));
	if (node == NULL) return NULL;

	node->id = 0;
	node->parent_id = 0;
	node->word = 0;
	node->usage = 0;
	node->count = 0;

	node->children = 0;
	node->nodes = NULL;

	return node;
}

int db_model_node_clear(db_tree *node) {
	number_t i;

	BUG_IF(node == NULL);

	node->id = 0;
	node->parent_id = 0;
	node->word = 0;
	node->usage = 0;
	node->count = 0;

	for (i = 0; i < node->children; i++)
		db_model_node_free((db_tree **)&node->nodes[i++]);
	free(node->nodes);

	node->children = 0;
	node->nodes = NULL;

	return OK;
}

void db_model_node_free(db_tree **node) {
	db_tree *node_p;
	number_t i;

	if (node == NULL || *node == NULL) return;

	node_p = *node;

	for (i = 0; i < node_p->children; i++)
		db_model_node_free((db_tree **)&node_p->nodes[i++]);
	free(node_p->nodes);

	free(*node);
	*node = NULL;
}
