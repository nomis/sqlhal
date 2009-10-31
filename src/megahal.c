#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"
#include "model.h"
#include "megahal.h"
#include "output.h"

static int megahal_learn(brain_t brain, list_t *input) {
	uint_fast32_t i;
	uint32_t size;
	number_t order;
	model_t *model;
	int ret;
	(void)brain;

	/* We only learn from inputs which are long enough */
	ret = db_model_get_order(brain, &order);
	if (ret) return ret;

	ret = list_size(input, &size);
	if (ret) return ret;

	if (size <= order) return OK;

	ret = model_alloc(brain, &model);
	if (ret) return ret;

	/*
	 * Train the model in the forwards direction. Start by initializing
	 * the context of the model.
	 */
	ret = model_init(model, MODEL_FORWARD);
	if (ret) return ret;

	for (i = 0; i < size; i++) {
		word_t word;

		/* Get word symbol. */
		ret = list_get(input, i, &word);
		if (ret) goto fail;

		/* Update the forward model. */
		ret = model_update(model, word, 1);
		if (ret) goto fail;
	}

	/*
	 * Add the sentence-terminating symbol.
	 */
	ret = model_update(model, 0, 1);
	if (ret) goto fail;

	/*
	 * Train the model in the backwards direction. Start by initializing
	 * the context of the model.
	 */
	ret = model_init(model, MODEL_BACKWARD);
	if (ret) return ret;

	for (i = 0; i < size; i++) {
		word_t word;

		/* Get word symbol. */
		ret = list_get(input, (size - 1) - i, &word);
		if (ret) goto fail;

		/* Update the backward model. */
		ret = model_update(model, word, 1);
		if (ret) goto fail;
	}

	/*
	 * Add the sentence-terminating symbol.
	 */
	ret = model_update(model, 0, 1);
	if (ret) goto fail;

	model_free(&model);
	return OK;

fail:
	model_free(&model);
	return ret;
}

static int megahal_timeout(struct timespec start) {
	struct timespec now;
	int64_t timeout = MEGAHAL_TIMEOUT_NS;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret) return 1;

	timeout -= (now.tv_sec - start.tv_sec) * 1000000000 + (now.tv_nsec - start.tv_nsec);
	return timeout <= 0;
}

static int megahal_reply(brain_t brain, list_t *input, list_t **output) {
	dict_t *keywords;
	list_t *current;
	struct timespec start;
	double surprise;
	double max_surprise;
	int ret;

	*output = NULL;

	ret = megahal_keywords(brain, input, &keywords);
	if (ret) return ret;

	ret = megahal_generate(brain, NULL, output);
	if (ret) {
		dict_free(&keywords);
		return ret;
	}

	if (!list_equal(input, *output))
		list_free(output);

	ret = clock_gettime(CLOCK_MONOTONIC, &start);
	if (ret) {
			ret = -ECLOCK;
			goto fail;
	}

	max_surprise = -1.0;
	do {
		ret = megahal_generate(brain, keywords, &current);
		if (ret) goto fail;

		ret = megahal_evaluate(brain, keywords, current, &surprise);
		if (ret) goto fail;

		if (surprise > max_surprise && !list_equal(input, current)) {
			max_surprise = surprise;
			list_free(output);
			*output = current;
		} else {
			list_free(&current);
		}
	} while(!megahal_timeout(start));

	dict_free(&keywords);

	return OK;

fail:
	list_free(output);
	dict_free(&keywords);
	return ret;
}

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags) {
	list_t *words_in;
	int ret;

	if (input != NULL) {
		char *tmp;

		tmp = strdup(input);
		if (tmp == NULL) return -ENOMEM;

		ret = megahal_parse(tmp, &words_in);
		free(tmp);
		if (ret) return ret;
	} else {
		words_in = NULL;
	}

	if ((flags & MEGAHAL_F_LEARN) != 0) {
		WARN_IF(words_in == NULL);

		ret = megahal_learn(brain, words_in);
		if (ret) {
			list_free(&words_in);
			return ret;
		}
	}

	if (output != NULL) {
		list_t *words_out;

		if (words_in == NULL) BUG(); // TODO

		ret = megahal_reply(brain, words_in, &words_out);
		list_free(&words_in);
		if (ret) return ret;

		if (words_out == NULL) {
			*output = strdup("I don't know enough to answer you yet!");
			if (*output == NULL) return -ENOMEM;
		} else {
			ret = megahal_output(words_out, output);
			list_free(&words_out);
			if (ret) return ret;
		}
	} else {
		list_free(&words_in);
	}
	return OK;
}

int megahal_train(brain_t brain, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	int ret = OK;

	WARN_IF(filename == NULL);

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;
		string = strtok(buffer, "\r\n");

		if (strlen(string) > 0) {
			ret = megahal_process(brain, buffer, NULL, MEGAHAL_F_LEARN);
			if (ret) goto fail;
		}
	}

fail:
	fclose(fd);
	return ret;
}
