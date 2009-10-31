#define _GNU_SOURCE

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"
#include "megahal.h"

static void megahal_capitalise(char *string) {
	size_t i, len;
	int start = 1;

	if (string == NULL) return;
	len = strlen(string);

	for (i = 0; i < len; i++) {
		if (isalpha((unsigned char)string[i])) {
			if (start) string[i] = (unsigned char)toupper((unsigned char)string[i]);
			else string[i] = (unsigned char)tolower((unsigned)string[i]);
			start = 0;
		}
		if ((i > 2) && (strchr("!.?", (unsigned char)string[i-1]) != NULL) && (isspace((unsigned char)string[i])))
			start = 1;
	}
}

static void megahal_upper(char *string) {
	size_t i, len;

	if (string == NULL) return;
	len = strlen(string);

	for (i = 0; i < len; i++)
		string[i] = (unsigned char)toupper((unsigned char)string[i]);
}

/* Return whether or not a word boundary exists in a string at the specified location. */
static int boundary(const char *string, uint_fast32_t position, uint_fast32_t len) {
	if (position == 0)
		return 0;

	if (position == len)
		return 1;

	if (
		(string[position] == '\'')
		&& (isalpha((unsigned char)string[position - 1]) != 0)
		&& (isalpha((unsigned char)string[position + 1]) != 0)
	)
		return 0;

	if (
		(position > 1)
		&& (string[position-1] == '\'')
		&& (isalpha((unsigned char)string[position - 2]) != 0)
		&& (isalpha((unsigned char)string[position]) != 0)
	)
		return 0;

	if (
		(isalpha((unsigned char)string[position]) != 0)
		&& (isalpha((unsigned char)string[position - 1]) == 0)
	)
		return 1;

	if (
		(isalpha((unsigned char)string[position]) == 0)
		&& (isalpha((unsigned char)string[position - 1]) != 0)
	)
		return 1;

	if (isdigit((unsigned char)string[position])
		!= isdigit((unsigned char)string[position - 1])
	)
		return 1;

	return 0;
}

int megahal_parse(const char *string, list_t **words) {
	list_t *words_p;
	uint_fast32_t offset, len;
	uint32_t size;
	word_t word;
	char *tmp;
	int ret;

	WARN_IF(string == NULL);
	WARN_IF(words == NULL);

	*words = list_alloc();
	if (*words == NULL) return -ENOMEM;
	words_p = *words;

	len = strlen(string);
	if (len == 0) return OK;

	offset = 0;
	while (1) {
		/*
		 * If the current character is of the same type as the previous
		 * character, then include it in the word. Otherwise, terminate
		 * the current word.
		 */
		if (boundary(string, offset, len)) {
			/*
			 * Add the word to the dictionary
			 */
			tmp = strndup(string, offset);
			if (tmp == NULL) return -ENOMEM;

			megahal_upper(tmp);

			ret = db_word_use(tmp, &word);
			free(tmp);
			if (ret) return ret;

			ret = list_append(words_p, word);
			if (ret) return ret;

			if (offset == len) break;
			string += offset;
			len = strlen(string);
			offset = 0;
		} else {
			offset++;
		}
	}

	/*
	 * If the last word isn't punctuation, then replace it with a
	 * full-stop character.
	 */
	tmp = NULL;
	ret = list_size(words_p, &size);
	if (ret) return ret;

	ret = list_get(words_p, size - 1, &word);
	if (ret) return ret;

	ret = db_word_str(word, &tmp);
	if (ret) return ret;

	if (isalnum((unsigned char)tmp[0])) {
		free(tmp);

		ret = db_word_use(".", &word);
		if (ret) return ret;

		ret = list_append(words_p, word);
		if (ret) return ret;
	} else if (strchr("!.?", (unsigned char)tmp[strlen(tmp) - 1]) == NULL) {
		free(tmp);

		ret = db_word_use(".", &word);
		if (ret) return ret;

		ret = list_set(words_p, size - 1, word);
		if (ret) return ret;
	} else {
		free(tmp);
	}

	return OK;
}

static int add_keyword(dict_t *keywords, word_t word) {
	int ret;
	char *tmp;

	ret = db_word_str(word, &tmp);
	if (ret) return ret;

	if (isalnum((unsigned char)tmp[0]) != 0)
		ret = dict_add(keywords, word, NULL);

	free(tmp);
	return ret;
}

int megahal_keywords(brain_t brain, const list_t *words, dict_t **keywords) {
	dict_t *keywords_p;
	uint_fast32_t i;
	uint32_t size;
	int ret;

	WARN_IF(words == NULL);
	WARN_IF(keywords == NULL);

	*keywords = dict_alloc();
	if (*keywords == NULL) return -ENOMEM;
	keywords_p = *keywords;

	ret = list_size(words, &size);
	if (ret) return ret;

	for (i = 0; i < size; i++) {
		word_t word;

		ret = list_get(words, i, &word);
		if (ret) return ret;

		ret = db_map_get(brain, MAP_SWAP, word, &word);
		if (ret != OK && ret != -ENOTFOUND) return ret;

		ret = db_list_contains(brain, LIST_BAN, word);
		if (ret == OK) continue;
		if (ret != -ENOTFOUND) return ret;

		ret = db_list_contains(brain, LIST_AUX, word);
		if (ret == OK) continue;
		if (ret != -ENOTFOUND) return ret;

		ret = db_model_contains(brain, word);
		if (ret == -ENOTFOUND) continue;
		if (ret != OK) return ret;

		ret = add_keyword(keywords_p, word);
		if (ret) return ret;
	}

	ret = dict_size(keywords_p, &size);
	if (ret) return ret;

	if (size == 0)
		return OK;

	ret = list_size(words, &size);
	if (ret) return ret;

	for (i = 0; i < size; i++) {
		word_t word;

		ret = list_get(words, i, &word);
		if (ret) return ret;

		ret = db_map_get(brain, MAP_SWAP, word, &word);
		if (ret != OK && ret != -ENOTFOUND) return ret;

		ret = db_list_contains(brain, LIST_AUX, word);
		if (ret == -ENOTFOUND) continue;
		if (ret != OK) return ret;

		ret = db_model_contains(brain, word);
		if (ret == -ENOTFOUND) continue;
		if (ret != OK) return ret;

		ret = add_keyword(keywords_p, word);
		if (ret) return ret;
	}

	return OK;
}

int megahal_output(const list_t *words, char **string) {
	size_t len = 0;
	uint_fast32_t i;
	uint32_t size;
	int ret;
	void *mem;

	WARN_IF(words == NULL);
	WARN_IF(string == NULL);
	WARN_IF(*string != NULL);

	ret = list_size(words, &size);
	if (ret) return ret;

	*string = malloc(sizeof(char) * (len + 1));
	if (*string == NULL) return -ENOMEM;
	(*string)[0] = 0;

	for (i = 0; i < size; i++) {
		word_t word;
		size_t tmp_len;
		char *tmp;

		ret = list_get(words, i, &word);
		if (ret) return ret;

		ret = db_word_str(word, &tmp);
		if (ret) return ret;

		tmp_len = strlen(tmp);

		mem = realloc(*string, sizeof(char) * (len + tmp_len + 1));
		if (mem == NULL) {
			free(*string);
			*string = NULL;
			free(tmp);
			return -ENOMEM;
		}
		*string = mem;
		(*string)[len + tmp_len] = 0;

		strcat(&(*string)[len], tmp);
		len += tmp_len;

		free(tmp);
	}

	megahal_capitalise(*string);
	return OK;
}
