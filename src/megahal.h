#define MEGAHAL_DEFAULT_ORDER 5
#define MEGAHAL_F_LEARN 0x01

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags);
int megahal_train(brain_t brain, const char *filename);

int megahal_parse(const char *string, list_t **words);
int megahal_output(list_t *words, char **string);
