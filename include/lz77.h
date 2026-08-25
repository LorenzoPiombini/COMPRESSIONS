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

struct Heap{
	struct Hnode *nodes;
	int32_t *idx; /*a pointer to the index array*/
	int16_t size;
};

struct Bit_writer{
	uint64_t bwritten; 
	uint64_t capacity;
	uint32_t accumulator;
	int nbits; /*how many bits are meaningful in accumulator*/
	uint8_t *buffer;
};

struct Bit_reader{
	uint64_t bwritten; 
	uint64_t capacity;
	uint32_t accumulator;
	int nbits; /*how many bits are meaningful in accumulator*/
	uint8_t *buffer;

};
long long deflate(uint8_t *input, uint64_t input_size,uint8_t **deflate_input);
/*TODO decode_LZ77 to inflate*/
void decode_LZ77(struct LDpair *pairs, uint64_t actual_pair, uint8_t *decoded_data);
#endif
