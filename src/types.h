enum list {
	LIST_AUX   = 1,
	LIST_BAN   = 2,
	LIST_GREET = 3
};
enum map {
	MAP_SWAP   = 4
};

typedef uint64_t brain_t;
typedef uint64_t word_t;
typedef uint64_t node_t;
typedef uint64_t tree_t;

typedef uint64_t number_t;

typedef struct {
	uint_fast32_t size;
	word_t *words;
} dict_t;

typedef struct {
	uint_fast32_t size;
	word_t *words;
} list_t;
