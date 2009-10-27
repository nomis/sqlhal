#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "dict.h"
#include "megahal.h"
#include "output.h"

int megahal_learn(brain_t brain, list_t *words) {
	(void)brain;
	(void)words;

	return -EFAULT;
}

int megahal_reply(brain_t brain, list_t *words, char **output) {
	(void)brain;
	(void)words;
	(void)output;

	BUG(); // TODO
}

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags) {
	list_t *words;
	int ret;

	printf("megahal_process %ld, %s, %p, %d\n", brain, input, output, flags);

	if (input != NULL) {
		char *tmp;

		tmp = strdup(input);
		if (tmp == NULL) return -ENOMEM;

		megahal_upper(tmp);
		ret = megahal_parse(tmp, &words);
		free(tmp);
		if (ret) return ret;
	} else {
		words = NULL;
	}

	if ((flags & MEGAHAL_F_LEARN) != 0) {
		WARN_IF(words == NULL);

		ret = megahal_learn(brain, words);
		list_free(&words);
		if (ret) return ret;
	}

	if (output != NULL) {
		if (words == NULL) BUG(); // TODO

		ret = megahal_reply(brain, words, output);
		list_free(&words);
		if (ret) return ret;
	}

	list_free(&words);
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
