#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lz77.h"


/* --------- Compressed block tables as per RFC-1951  -----*/

static const uint16_t length_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t length_extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t distance_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t distance_extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static uint16_t len_to_code[259];
static uint16_t dist_to_code_low[257];
static uint16_t dist_to_code_rest[256];
/* ------------------------------------------------------ */

static uint32_t hash3(uint8_t *p);
static void insert(struct LZstate *state,uint8_t *p, uint64_t pos);
static void gen_len_to_code_table(uint16_t *table);
static uint8_t dist_code(uint32_t d);
static uint16_t len_code(uint16_t l);
static int is_node_empty(struct Hnode *n);

/*---- Heap tree functions -----*/

static void heap_swap_L(struct Heap_literal *h,int a, int b);
static void heap_swap_D(struct Heap_distance *h,int a, int b);
static uint32_t heap_freq_L(struct Heap_literal *h,int a);
static uint32_t heap_freq_D(struct Heap_distance *h,int a);
static void sift_heap_down_L(struct Heap_literal *h,int i);
static void sift_heap_down_D(struct Heap_distance *h,int i);
static void sift_heap_up_L(struct Heap_literal *h,int i);
static void sift_heap_up_D(struct Heap_distance *h,int i);
static int16_t heap_pop_L(struct Heap_literal *h);
static int16_t heap_pop_D(struct Heap_distance *h);	
static void heap_push_L(struct Heap_literal *h,int16_t i);
static void heap_push_D(struct Heap_distance *h,int16_t i);	


/*-----------------------------------*/

void assign_depth(struct Hnode *n,int16_t i, int8_t depth, uint8_t *code_len)
{
	if(n[i].left < 0){
		code_len[n[i].symbol] = depth;
		return;
	}

	assign_depth(n,n[i].left,depth + 1,code_len);
	assign_depth(n,n[i].rigth,depth + 1,code_len);
}


static int16_t heap_pop_L(struct Heap_literal *h)
{
	int16_t top = h->idx[0]; 
	h->idx[0] = h->idx[--h->size];
	if(h->size) sift_heap_down_L(h,0);
	return top;
}

static int16_t heap_pop_D(struct Heap_distance *h)
{
	int16_t top = h->idx[0]; 
	h->idx[0] = h->idx[--h->size];
	if(h->size) sift_heap_down_D(h,0);
	return top;

}

static void heap_push_L(struct Heap_literal *h,int16_t i)
{
	h->idx[h->size++] = i; 
	sift_heap_up_L(h,h->size -1);
}
static void heap_push_D(struct Heap_distance *h,int16_t i)
{
	h->idx[h->size++] = i; 
	sift_heap_up_D(h,h->size -1);
}



static void sift_heap_up_L(struct Heap_literal *h,int i)
{
	while(i > 0){
		int p = (i -1) / 2;
		if(heap_freq_L(h,p) <= heap_freq_L(h,i)) return;
		heap_swap_L(h, i, p);
		i = p;
	}
}

static void sift_heap_up_D(struct Heap_distance *h,int i)
{
	while(i > 0){
		int p = (i -1) / 2;
		if(heap_freq_D(h,p) <= heap_freq_D(h,i)) return;
		heap_swap_D(h, i, p);
		i = p;
	}
}

static void sift_heap_down_D(struct Heap_distance *h,int i)
{
	for(;;){
		int l = i*2+1;
		int r = i*2+2;
		int small = i;
		if(l < h->size && heap_freq_D(h,l) < heap_freq_D(h,small)) small = l;
		if(r < h->size && heap_freq_D(h,r) < heap_freq_D(h,small)) small = r;
		if(small == i) return;
		heap_swap_D(h,i,small);
		i = small;
	}
}

static void sift_heap_down_L(struct Heap_literal *h,int i)
{
	for(;;){
		int l = i*2+1;
		int r = i*2+2;
		int small = i;
		if(l < h->size && heap_freq_L(h,l) < heap_freq_L(h,small)) small = l;
		if(r < h->size && heap_freq_L(h,r) < heap_freq_L(h,small)) small = r;
		if(small == i) return;
		heap_swap_L(h,i,small);
		i = small;
	}
}

static void heap_swap_L(struct Heap_literal *h,int a, int b)
{
	int16_t t = h->idx[a];
	h->idx[a] = h->idx[b];
	h->idx[b] = t;
}

static void heap_swap_D(struct Heap_distance *h,int a, int b)
{
	int16_t t = h->idx[a];
	h->idx[a] = h->idx[b];
	h->idx[b] = t;
}

static uint32_t heap_freq_D(struct Heap_distance *h,int a)
{
	return h->nodes[h->idx[a]].freq;
}

static uint32_t heap_freq_L(struct Heap_literal *h,int a)
{
	return h->nodes[h->idx[a]].freq;
}

static uint32_t hash3(uint8_t *p)
{
	return ((p[0] << 10) ^ (p[1] << 5) ^ p[2]) & (HASH_SIZE - 1);
}

void LZstate_init(struct LZstate *state)
{	
	memset(state->head,0xFF,sizeof state->head);
	state->max_chain = MAX_CHAIN;
}

static void insert(struct LZstate *state,uint8_t *p, uint64_t pos)
{
	uint32_t h = hash3(p+pos);
	state->prev[pos & (WINDOW_SIZE - 1)] = state->head[h];
	state->head[h] = (int32_t)pos;
}

static void gen_len_to_code_table(uint16_t *table)
{
	for(uint16_t i = 0; i < 29; i++){
		uint16_t hi = (i == 28) ? 258 : length_base[i+1] - 1;
		for(uint16_t len = length_base[i]; len <= hi; len++){
			table[len] = 257+i;
		}
	}
}

static uint16_t len_code(uint16_t l)
{
	return len_to_code[l];
}

static void gen_dist_to_code_tables(uint16_t *lo_table, uint16_t *hi_table)
{
	for(int i = 0; i < 30; i++){
		uint32_t hi = (i == 29) ? WINDOW_SIZE : distance_base[i+1] -1;	
		for(uint32_t dist = distance_base[i]; dist <= hi; dist++){
			if(dist <= 256){
				dist_to_code_low[dist] = i; 
			}else{
				dist_to_code_rest[(dist - 1 ) >> 7] =  i;
			}
		}
	}
}

static uint8_t dist_code(uint32_t d)
{
	return (d <= 256) ? dist_to_code_low[d] : dist_to_code_rest[(d-1)>>7];
}

int debug_tb(){
	gen_len_to_code_table(len_to_code);
	for(int len = 3; len <= 258; len++){
		int c = len_code(len);
		int i = c - 257;
		int lo = length_base[i];
		int hi = (i == 28) ? 258 : length_base[i] + (1 << length_extra[i]) - 1;
		if(len < lo || len > hi) printf("BAD len %d -> code %d\n", len, c);
	}

	gen_dist_to_code_tables(dist_to_code_low,dist_to_code_rest);
	for(int dist = 1; dist <= 32768; dist++){
		int c = dist_code(dist);
		int lo = distance_base[c];
		int hi = (c == 29) ? 32768 : distance_base[c] + (1 << distance_extra[c]) - 1;
		if(dist < lo || dist > hi) printf("BAD dist %d -> code %d\n", dist, c);
	}
	return 0;
}



int16_t lit_tree(uint32_t *lit_freq,struct Hnode *n,struct Heap_literal *h)
{
	int n_count = 0;
	for(int i = 0; i < 286; i++){
		if(lit_freq[i] == 0) continue;
		n[n_count].symbol = i;
		n[n_count].freq = lit_freq[i];
		n[n_count].left = -1;
		n[n_count].rigth = -1;
		h->idx[n_count] = n_count;
		n_count++;
	}
	h->size = n_count;
	(*h).nodes = n;

	for(int i = h->size/2 -1 ; i >= 0; i --)
		sift_heap_down_L(h,i);

	while(h->size > 1){
		int16_t a = heap_pop_L(h);
		int16_t b = heap_pop_L(h);
		int16_t p = n_count++;
		n[p].freq = n[a].freq + n[b].freq;
		n[p].left = a;
		n[p].rigth = b;
		n[p].symbol = 0xffff;
		heap_push_L(h,p);
	}

	return heap_pop_L(h);/*root*/
}
void count_frequency(struct LDpair *pairs, uint64_t tokens,uint32_t *lit_freq,uint32_t *dist_freq)
{
	memset(len_to_code,0,sizeof(len_to_code));
	memset(dist_to_code_low,0,sizeof(dist_to_code_low));
	memset(dist_to_code_rest,0,sizeof(dist_to_code_rest));


	gen_len_to_code_table(len_to_code);
	gen_dist_to_code_tables(dist_to_code_low,dist_to_code_rest);

	for(uint64_t k = 0; k < tokens;k++){
		if(pairs[k].length == 0){
			lit_freq[pairs[k].literal]++;
		}else{
			lit_freq[len_code(pairs[k].length)]++;
			dist_freq[dist_code(pairs[k].distance)]++;
		}
	}
	lit_freq[256]++;
}

void find_match(struct LZstate *state,uint8_t* base,size_t bread,size_t remain, uint16_t *out_len, uint16_t *out_dist)
{
	uint64_t best_distance = 0, best_length = 0;
	uint8_t *cur = base + bread;
	uint64_t max_len = remain < MAX_MATCH ? remain : MAX_MATCH;

	if(remain < MIN_MATCH){
		*out_len = *out_dist = 0;
		return;
	}

	int32_t cand = state->head[hash3(cur)];
	int32_t tries = state->max_chain;
	while(cand >= 0 && tries--){
		uint64_t dist = bread - cand;
		if(dist > WINDOW_SIZE) break;

		uint64_t current_l = 0;
		while(current_l < max_len && base[cand + current_l] == cur[current_l])
			current_l++;
	
		if(current_l > best_length){
			best_length = current_l;
			best_distance = dist; 
			if(current_l == max_len) break;
		}
		cand = state->prev[cand & (WINDOW_SIZE -1)];
	}
	if(best_length < MIN_MATCH){
		best_length = best_distance = 0;
	}

	*out_len = (uint16_t)best_length;
	*out_dist = (uint16_t)best_distance;
}

int LZ77_binary(uint8_t *input,struct LDpair **pairs)
{
	uint64_t *pairs_size = ((uint64_t*)(*pairs) - 1);
	uint64_t input_size = *((uint64_t*)input - 1);

	struct LZstate state = {0};
	LZstate_init(&state);
	uint64_t i = 0;
	uint64_t bread = 0;

	while(bread < input_size) {
		if(i >= *pairs_size){
			struct LDpair *np = realloc(pairs_size,sizeof(uint64_t) + (sizeof *pairs * (*pairs_size) * 2));
			if(!np) return -1;
			pairs_size = (uint64_t*)np;
			*pairs = (struct LDpair*)(pairs_size + 1);
			*pairs_size = *pairs_size * 2;
		}

		uint16_t dist,len;
		find_match(&state,input,bread,input_size - bread,&len,&dist);
		uint16_t step = (len == 0 ) ? 1 : len;
		for(uint64_t k = 0; k < step; k++){
			if(bread + k + MIN_MATCH <= input_size){
				insert(&state,input,bread+k);
			}
		}

		if(len == 0){
			(*pairs)[i].length = 0;
			(*pairs)[i].literal = input[bread];
		}else{
			(*pairs)[i].length = len;
			(*pairs)[i].distance = dist;
		}
		bread += step;
		i++;
	}
	return (int)i;
}

void decode_LZ77(struct LDpair *pairs, uint64_t actual_pair, uint8_t *decoded_data)
{
	uint64_t pos = 0;
	uint64_t i;
	for(i = 0; i < actual_pair;i++){
		if(pairs[i].length == 0){
			decoded_data[pos++] = pairs[i].literal;
		}else{
			uint64_t from = pos - pairs[i].distance;
			uint64_t j;
			for(j = 0; j < pairs[i].length; j++){
				decoded_data[pos++] = decoded_data[from++];
			}
		}
	}
}

