#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "err.h"
#include "db.h"
#include "megahal.h"
#include "output.h"

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags) {
	printf("megahal_process %ld, %s, %p, %d\n", brain, input, output, flags);
	// TODO
	return -EFAULT;
}

int megahal_train(brain_t brain, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	int ret = OK;

	if (filename == NULL) return -EINVAL;

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
