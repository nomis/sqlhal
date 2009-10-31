#include <stdint.h>
#include <stdlib.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"
#include "megahal.h"

int megahal_generate(brain_t brain, const dict_t *keywords, list_t **words) {
	(void)brain;
	(void)keywords;
	(void)words;

	BUG(); // TODO
}

int megahal_evaluate(brain_t brain, const dict_t *keywords, const list_t *words, double *surprise) {
	(void)brain;
	(void)keywords;
	(void)words;
	(void)surprise;

	BUG(); // TODO
}
