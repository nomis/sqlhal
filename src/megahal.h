#define MEGAHAL_DEFAULT_ORDER 5
#define MEGAHAL_TIMEOUT_NS 1000000000
#define MEGAHAL_F_LEARN 0x01

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags);
int megahal_train(brain_t brain, const char *filename);

int megahal_parse(const char *string, list_t **words);
int megahal_keywords(brain_t brain, const list_t *words, dict_t **keywords);
int megahal_generate(brain_t brain, const dict_t *keywords, list_t **words);
int megahal_evaluate(brain_t brain, const dict_t *keywords, const list_t *words, double *surprise);
int megahal_output(const list_t *words, char **string);
