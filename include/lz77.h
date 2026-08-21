#ifndef __LZ77__H
#define __LZ77__H 1

#define MIN_MATCH 3
#define MAX_MATCH 258
#define WINDOW_SIZE 32768

#define MAX_CHAIN 4096
#define HASH_BIT 15
#define HASH_SIZE (1 << HASH_BIT)

struct LZstate{
	int16_t head[HASH_SIZE];
	int16_t prev[WINDOW_SIZE];
	uint32_t max_chain;
};

struct LDpair {
	uint16_t length;
	uint16_t distance;
	uint8_t literal;
};

int debug_tb();
void LZstate_init(struct LZstate *state);
void find_match(struct LZstate *state,uint8_t* base,size_t bread,size_t remain, uint16_t *out_len, uint16_t *out_dist);
int LZ77_binary(uint8_t *input,struct LDpair **pairs);
void decode_LZ77(struct LDpair *pairs, uint64_t actual_pair, uint8_t *decoded_data);
#endif
