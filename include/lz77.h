#ifndef __LZ77__H
#define __LZ77__H 1

#define MIN_MATCH 3
#define MAX_MATCH 258
#define WINDOW_SIZE 32768

#define MAX_CHAIN 4096
#define HASH_BIT 15
#define HASH_SIZE (1 << HASH_BIT)

#define MAX_LIT_TREE ((286 * 2) -1)
#define MAX_DIST_TREE ((30*2) - 1)

struct LZstate{
	int32_t head[HASH_SIZE];
	int32_t prev[WINDOW_SIZE];
	uint32_t max_chain;
};

struct LDpair {
	uint16_t length;
	uint16_t distance;
	uint8_t literal;
};

struct Hnode{
	uint32_t freq;
	int16_t left,rigth;
	uint16_t symbol;
};

struct Heap_literal{
	int16_t idx[MAX_LIT_TREE];
	int16_t size;
	struct Hnode *nodes;
};

struct Heap_distance{
	uint16_t idx[MAX_DIST_TREE];
	int16_t size;
	struct Hnode *nodes;
};

int debug_tb();
void assign_depth(struct Hnode *n,int16_t i, int8_t depth, uint8_t *code_len);
int16_t lit_tree(uint32_t *lit_freq,struct Hnode *n,struct Heap_literal *h);
void count_frequency(struct LDpair *pairs, uint64_t tokens,uint32_t *lit_freq,uint32_t *dist_freq);
void LZstate_init(struct LZstate *state);
void find_match(struct LZstate *state,uint8_t* base,size_t bread,size_t remain, uint16_t *out_len, uint16_t *out_dist);
int LZ77_binary(uint8_t *input,struct LDpair **pairs);
void decode_LZ77(struct LDpair *pairs, uint64_t actual_pair, uint8_t *decoded_data);
void smallest_lit_freq(struct LDpair *pairs,uint64_t tokens,uint32_t *lit_freq,uint32_t *dist_freq,uint64_t *smallest,uint64_t *second_smallest);
#endif
