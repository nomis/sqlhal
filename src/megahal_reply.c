#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"
#include "model.h"
#include "megahal.h"

static int seed(brain_t brain, const model_t *model, const dict_t *keywords, word_t *word) {
	uint32_t size;
	int ret;

	ret = model_rand_word(model, word);
	if (ret == -ENOTFOUND) *word = 0;
	else if (ret) return ret;

	if (keywords == NULL)
		return OK;

	ret = dict_size(keywords, &size);
	if (ret) return ret;

	if (size > 0) {
		uint_fast32_t i, offset;

		offset = random() % size;

		for (i = 0; i < size; i++) {
			word_t tmp;

			ret = dict_get(keywords, (i + offset) % size, &tmp);
			if (ret) return ret;

			ret = db_model_contains(brain, tmp);
			BUG_IF(ret != OK);
/*
			if (ret == -ENOTFOUND) continue;
			if (ret != OK) return ret;
*/

			ret = db_list_contains(brain, LIST_AUX, tmp);
			if (ret == OK) continue;
			if (ret != -ENOTFOUND) return ret;

			*word = tmp;
			break;
		}
	}

	return OK;
}

static int babble(brain_t brain, const model_t *model, const dict_t *keywords, const list_t *words, int *use_aux, word_t *word) {
	model_rand_t state;
	int ret;

	ret = model_rand_init(model, &state);
	if (ret == -ENOTFOUND) *word = 0;
	else if (ret) return ret;

	*word = 0;
	if (keywords == NULL)
		return -ENOTFOUND;

	while (1) {
		ret = model_rand_next(&state, word);
		if (ret == -ENOTFOUND) break;
		if (ret != OK) goto fail;

		ret = dict_find(keywords, *word, NULL);
		if (ret == -ENOTFOUND) continue;
		if (ret != OK) goto fail;

		if (!*use_aux) {
			ret = db_list_contains(brain, LIST_AUX, *word);
			if (ret == OK) continue;
			if (ret != -ENOTFOUND) goto fail;
		}

		ret = list_contains(words, *word);
		if (ret == OK) continue;
		if (ret != -ENOTFOUND) goto fail;

		*use_aux = 1;
		break;
	}
	model_rand_free(&state);

	if (*word == 0)
		return -ENOTFOUND;
	return OK;

fail:
	model_rand_free(&state);
	return ret;
}

int megahal_generate(brain_t brain, const dict_t *keywords, list_t **words) {
	number_t order;
	model_t *model;
	list_t *words_p;
	uint32_t size;
	int start = 1;
	int use_aux = 0;
	int ret;

	ret = db_model_get_order(brain, &order);
	if (ret) return ret;

	ret = model_alloc(brain, &model);
	if (ret) goto fail;

	*words = list_alloc();
	if (*words == NULL) return -ENOMEM;
	words_p = *words;

	/* Generate the reply in the forward direction. */
	ret = model_init(model, MODEL_FORWARD);
	if (ret) goto fail;

	while (1) {
		word_t word;

		/* Get a random symbol from the current context. */
		if (start) {
			ret = seed(brain, model, keywords, &word);
			if (ret == -ENOTFOUND) break;
			if (ret != OK) goto fail;
			start = 0;
		} else {
			ret = babble(brain, model, keywords, words_p, &use_aux, &word);
			if (ret == -ENOTFOUND) break;
			if (ret != OK) goto fail;

			/* Allow an auxilliary keyword if a normal keyword has
			 * already been used.
			 */
			use_aux = 1;
		}

		/* Append the symbol to the reply dictionary. */
		ret = list_append(words_p, word);
		if (ret) goto fail;

		/* Extend the current context of the model with the current symbol. */
		ret = model_update(model, word, 0);
		if (ret) goto fail;
	}

	/* Start off by making sure that the model's context is empty. */
	ret = model_init(model, MODEL_BACKWARD);
	if (ret) goto fail;

	/*
	 * Re-create the context of the model from the current reply
	 * dictionary so that we can generate backwards to reach the
	 * beginning of the string.
	 */
	ret = list_size(words_p, &size);
	if (ret) goto fail;

	if (size > 0) {
		uint_fast32_t i;
		for (i = 0; i < ((order + 1) < size ? (order + 1) : size); i++) {
			word_t word;

			ret = list_get(words_p, i, &word);
			if (ret) goto fail;

			ret = model_update(model, word, 0);
			if (ret) goto fail;
		}
	}

	/* Generate the reply in the backward direction. */
	while (1) {
		word_t word;

		/* Get a random symbol from the current context. */
		ret = babble(brain, model, keywords, words_p, &use_aux, &word);
		if (ret == -ENOTFOUND) break;
		if (ret != OK) goto fail;

		/* Allow an auxilliary keyword if a normal keyword has
		 * already been used.
		 */
		use_aux = 1;

		/* Prepend the symbol to the reply dictionary. */
		ret = list_prepend(words_p, word);
		if (ret) goto fail;

		/* Extend the current context of the model with the current symbol. */
		ret = model_update(model, word, 0);
		if (ret) goto fail;
	}

	return OK;

fail:
	list_free(words);
	model_free(&model);
	return ret;
}

int megahal_evaluate(brain_t brain, const dict_t *keywords, const list_t *words, double *surprise) {
	(void)brain;
	(void)keywords;
	(void)words;
	(void)surprise;

	char *tmp = NULL;
	megahal_output(words, &tmp);
	printf("%s\n", tmp);
	free(tmp);

	*surprise = 0;
	return OK;
	BUG(); // TODO
}
