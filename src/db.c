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
