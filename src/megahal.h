#define MEGAHAL_LEARN 1
#define MEGAHAL_REPLY 2

int megahal_switch_brain(const char *name);
int megahal_switch_base(const char *name);

int megahal_process(const char *input, char **output, uint8_t flags);
int megahal_train(const char *base, const char *filename);
