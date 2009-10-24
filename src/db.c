#include <stdint.h>
#include <stdlib.h>

#include "err.h"
#include "types.h"
#include "db.h"

int db_brain_use(const char *brain, brain_t *ref) {
	int ret;

	ret = db_brain_get(brain, ref);
	if (ret == -ENOTFOUND)
		ret = db_brain_add(brain, ref);
	return ret;
}

int db_word_use(const char *word, word_t *ref) {
	int ret;

	if (word == NULL || word[0] == 0) return -EINVAL;

	ret = db_word_get(word, ref);
	if (ret == -ENOTFOUND)
		ret = db_word_add(word, ref);
	return ret;
}
