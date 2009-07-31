#include <stdint.h>

#include "err.h"
#include "types.h"
#include "db.h"

int db_word_use(const char *word, word_t *ref) {
	int ret;

	ret = db_word_get(word, ref);
	if (ret == -ENOTFOUND)
		ret = db_word_add(word, ref);
	return ret;
}

int db_map_use(db_hand **hand, word_t *key, word_t *value) {
	int ret;

	ret = db_map_get(hand, key, value);
	if (ret == -ENOTFOUND)
		ret = db_map_put(hand, key, value);
	return ret;
}
