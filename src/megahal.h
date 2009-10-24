#define MEGAHAL_LEARN 1
#define MEGAHAL_REPLY 2

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags);
int megahal_train(brain_t brain, const char *filename);
