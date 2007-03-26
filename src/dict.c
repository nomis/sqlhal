#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"


int initialise_list(const char *personality, const char *type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	char *list;
	int ret = OK;

	if (personality == NULL || type == NULL || filename == NULL) return -EINVAL;

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	list = malloc((strlen(personality) + strlen(type) + 2) * sizeof(char));
	sprintf(list, "%s_%s", personality, type);
	ret = db_list_init(list);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;
		string = strtok(buffer, "\t \n#");

		if ((string != NULL) && (strlen(string) > 0)) {
			word_t word;

			ret = db_word_get(string, &word);
			if (ret == -ENOTFOUND)
				ret = db_word_add(string, &word);
			if (ret) goto fail;

			ret = db_list_contains(list, &word);
			if (ret == -ENOTFOUND)
				ret = db_list_add(list, &word);
			if (ret) goto fail;
		}
	}

	goto end;

fail:
	printf("err: %d\n", ret);

end:
	fclose(fd);
	return ret;
}
