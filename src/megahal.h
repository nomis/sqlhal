#define MEGAHAL_DEFAULT_ORDER 5
#define MEGAHAL_F_LEARN 0x01

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags);
int megahal_train(brain_t brain, const char *filename);

void megahal_capitalise(char *string);
void megahal_upper(char *string);
int megahal_parse(const char *string, list_t **words);
