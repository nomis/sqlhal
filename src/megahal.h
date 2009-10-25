#define MEGAHAL_F_LEARN 0x01

int megahal_process(brain_t brain, const char *input, char **output, uint8_t flags);
int megahal_train(brain_t brain, const char *filename);
