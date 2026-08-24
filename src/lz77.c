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
static void count_frequency(struct LDpair *pairs, uint64_t tokens,uint32_t *lit_freq,uint32_t *dist_freq);

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


static int16_t lit_tree(uint32_t *lit_freq,struct Hnode *n,struct Heap_literal *h);
static int16_t dist_tree(uint32_t *dist_freq,struct Hnode *n, struct Heap_distance *h);
/*-----------------------------------*/

/*-----------Huffman Coding---------*/
static void assign_depth(struct Hnode *n,int16_t i, int8_t depth, uint8_t *code_len);
static void gen_codes(uint8_t *code_len, int16_t n, uint16_t *codes);
static void gen_huffman_codes(uint32_t *lit_freq,uint32_t *dist_freq,uint16_t *lit_codes,uint16_t *dist_codes,uint8_t *code_len,uint8_t *code_len_dist);
static int put_bits(struct Bit_writer *w,uint32_t value,int n);
static int put_code(struct Bit_writer *w,uint64_t code,int len);
static int flush(struct Bit_writer *w);


/*-----------------------------------*/

static int flush(struct Bit_writer *w)
{
	while(w->nbits > 0){
		if(w->bwritten == w->capacity){
			uint8_t *np = realloc(w->buffer,(w->capacity*2) * sizeof *np);
			if(!np) return -1;
			w->buffer = np;
			w->capacity *= 2; 
		}
		w->buffer[w->bwritten++] = w->accumulator & 0xFF;
		w->accumulator >>= 8;
		w->nbits -= 8;
	}

	return 0;
}

static int put_bits(struct Bit_writer *w,uint32_t value,int n)
{
	w->accumulator |= (value & ((1u << n) -1)) << w->nbits;
	w->nbits += n;
	while(w->nbits >= 8){
		if(w->bwritten == w->capacity){
			uint8_t *np = realloc(w->buffer,(w->capacity*2) * sizeof *np);
			if(!np) return -1;
			w->buffer = np;
			w->capacity *= 2; 
		}
		w->buffer[w->bwritten++] = w->accumulator & 0xFF;
		w->accumulator >>= 8;
		w->nbits -= 8;
	}
	return 0;
}

static int put_code(struct Bit_writer *w,uint64_t code,int len)
{
	uint16_t rev = 0;
	for(int i = 0; i <len ; i++){
		rev |= ((code >> i) & 1) << (len - 1 - i);
	}
	if(put_bits(w,rev,len) == -1) return -1;
	return 0;
}


static void assign_depth(struct Hnode *n,int16_t i, int8_t depth, uint8_t *code_len)
{
	if(n[i].left < 0){
		code_len[n[i].symbol] = depth;
		return;
	}

	assign_depth(n,n[i].left,depth + 1,code_len);
	assign_depth(n,n[i].rigth,depth + 1,code_len);
}

static void gen_codes(uint8_t *code_len, int16_t n, uint16_t *codes)
{
	uint16_t bl_count[16] = {0};
	int i;
	for(i = 0; i < n; i++){
		bl_count[code_len[i]]++;
	}

	bl_count[0] = 0;

	uint16_t next_code[16] = {0}, code = 0;
	for(int bits = 1; bits <= 15; bits++){
		code = (code + bl_count[bits-1]) << 1;
		next_code[bits] = code;
	}

	for(i = 0; i < n;i++){
		if(code_len[i]) codes[i] = next_code[code_len[i]]++;
	}

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



static int16_t dist_tree(uint32_t *dist_freq,struct Hnode *n, struct Heap_distance *h)
{
	int n_count = 0;
	for(int i = 0; i < 30; i++){
		if(dist_freq[i] == 0) continue;
		n[n_count].symbol = i;
		n[n_count].freq = dist_freq[i];
		n[n_count].left = -1;
		n[n_count].rigth = -1;
		h->idx[n_count] = n_count;
		n_count++;
	}

	h->size = n_count;
	(*h).nodes = n;

	for(int i = h->size/2 -1 ; i >= 0; i --)
		sift_heap_down_D(h,i);

	while(h->size > 1){
		int16_t a = heap_pop_D(h);
		int16_t b = heap_pop_D(h);
		int16_t p = n_count++;
		n[p].freq = n[a].freq + n[b].freq;
		n[p].left = a;
		n[p].rigth = b;
		n[p].symbol = 0xffff;
		heap_push_D(h,p);
	}

	return heap_pop_D(h);/*root*/
}

static int16_t lit_tree(uint32_t *lit_freq,struct Hnode *n,struct Heap_literal *h)
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

static void gen_huffman_codes(uint32_t *lit_freq,uint32_t *dist_freq,uint16_t *lit_codes,uint16_t *dist_codes,uint8_t *code_len,uint8_t *code_len_dist)
{
	struct Hnode n[MAX_LIT_TREE] = {0};
	struct Heap_literal h = {0};
	struct Heap_distance hd = {0};
	struct Hnode nd[MAX_DIST_TREE] = {0};


	int16_t root_l = lit_tree(lit_freq,n,&h);
	int16_t root_d = dist_tree(dist_freq,nd,&hd);

	assign_depth(n,root_l,0,code_len);
	assign_depth(nd,root_d,0,code_len_dist);


	gen_codes(code_len,286,lit_codes);
	gen_codes(code_len_dist,30,dist_codes);

#if 0

	/*NOTE THESE TWO LOOPS ARE A TEST TO VALIDATE THE TREES*/
	for(int a = 0; a < 286; a++){
		if(!code_len[a]) continue;
		for(int b = 0; b < 286; b++){
			if(a == b || !code_len[b]) continue;
			if(code_len[a] <= code_len[b]){
				int shift = code_len[b] - code_len[a];
				if((lit_codes[b] >> shift) == lit_codes[a])
					printf("PREFIX VIOLATION: %d is prefix of %d\n", a, b);
			}
		}
	}

	for(int a = 0; a < 30; a++){
		if(!code_len_dist[a]) continue;
		for(int b = 0; b < 30; b++){
			if(a == b || !code_len_dist[b]) continue;
			if(code_len_dist[a] <= code_len_dist[b]){
				int shift = code_len_dist[b] - code_len_dist[a];
				if((dist_codes[b] >> shift) == dist_codes[a])
					printf("PREFIX VIOLATION: %d is prefix of %d\n", a, b);
			}
		}
	}
#endif
}
static void count_frequency(struct LDpair *pairs, uint64_t tokens,uint32_t *lit_freq,uint32_t *dist_freq)
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

long long deflate(uint8_t *input, uint64_t input_size,uint8_t **deflate_input)
{
	size_t pair_size = input_size / sizeof(struct LDpair);
	pair_size *= sizeof(struct LDpair);

	uint64_t *pairs = (uint64_t*)malloc(sizeof(uint64_t) + (sizeof(struct LDpair) *pair_size));
	if(!pairs) return -1;
	memset(pairs,0,sizeof(uint64_t) + ((sizeof(struct LDpair) * pair_size)));
	*pairs = pair_size;
	struct LDpair *p = (struct LDpair *)( pairs + 1);
	uint64_t tokens = LZ77_binary(input,&p);

	uint32_t lit_freq[286] = {0};
	uint32_t dist_freq[30] = {0};

	count_frequency(p, tokens,lit_freq,dist_freq);

	uint16_t lit_codes[286] = {0};
	uint16_t dist_codes[30] = {0};
	uint8_t code_len[286] = {0};
	uint8_t code_len_dist[30] = {0};

	gen_huffman_codes(lit_freq,dist_freq,lit_codes,dist_codes,code_len,code_len_dist);

	struct Bit_writer w = {0};
	w.buffer = malloc(1024*4); /*4 kib*/
	if(!w.buffer) goto clean_on_failure;

	memset(w.buffer,0,1024*4);
	w.capacity = 1024*4;

	if(put_bits(&w,1,1) == -1) goto clean_on_failure;
	if(put_bits(&w,2,2) == -1) goto clean_on_failure;

	/*HEADER*/
	int hlit = 286;
	while(hlit > 257 && code_len[hlit - 1] == 0) hlit--;

	int hdist = 30;
	while(hdist > 1 && code_len_dist[hdist - 1] == 0) hdist--;

	uint8_t all_len[286 + 30] = {0};
	int n_all = 0;
	for(int i = 0; i < hlit;  i++) all_len[n_all++] = code_len[i];
	for(int i = 0; i < hdist; i++) all_len[n_all++] = code_len_dist[i];

	uint8_t cl_sym[316] = {0}; 
	uint16_t cl_extra[316] = {0}; 

	int i = 0, n_cl = 0;
	while(i < n_all){
		uint8_t v = all_len[i];
		int run = 1;
		while(run + i < n_all && (all_len[i+run] == v)) run++;

		if(v == 0){
			while(run >= 3){
				int r = run > 138 ? 138 : run;
				if(r >= 11){
					cl_sym[n_cl] = 18;
					cl_extra[n_cl++] = r - 11;
				} else {
					cl_sym[n_cl] = 17;
					cl_extra[n_cl++] = r - 3;
				}
				run -= r;
				i += r;
			}
			while(run--){n_cl++;i++;}
		}else{
			cl_sym[n_cl] = v;
			n_cl++;
			i++;
			run--;
			while(run >= 3){
				int r = run > 6 ? 6 : run;
				cl_sym[n_cl] = 16;
				cl_extra[n_cl++] = r -3;
				run -= r;
				i += r;
			}
			while(run--){
				cl_sym[n_cl] = v;
				n_cl++;
				i++;
			}
		}
	}

	if(put_bits(&w,hlit - 257,5) == -1) goto clean_on_failure;
	if(put_bits(&w,hdist - 1,5) == -1) goto clean_on_failure;

	for(uint64_t i = 0; i < tokens; i++){
		if(p[i].length == 0){
			uint8_t lit = p[i].literal;
			if(put_code(&w,lit_codes[lit],code_len[lit]) == -1) goto clean_on_failure;
		}else{
			uint16_t lc = len_code(p[i].length);
			int	li =  lc - 257;
			if(put_code(&w,lit_codes[lc],code_len[lc]) == -1) goto clean_on_failure;
			if(length_extra[li])
				if(put_bits(&w,p[i].length - length_base[li],length_extra[li]) == -1) goto clean_on_failure;

			uint8_t dc = dist_code(p[i].distance);
			if(put_code(&w,dist_codes[dc],code_len_dist[dc]) == -1) goto clean_on_failure;

			if(distance_extra[dc])
				if(put_bits(&w,p[i].distance - distance_base[dc],distance_extra[dc]) == -1) goto clean_on_failure;
		}
	}

	if(put_code(&w,lit_codes[256],code_len[256]) == -1) goto clean_on_failure;
	flush(&w);

#if 0
	/*NOTE: THIS IS A TEST for the bit writer*/
	uint64_t bits = 0;
	for(int s = 0; s < 286; s++) bits += (uint64_t)lit_freq[s] * code_len[s];
	for(int s = 0; s < 30;  s++) bits += (uint64_t)dist_freq[s] * code_len_dist[s];
	/* plus extra bits */
	for(uint64_t k = 0; k < tokens; k++){
		if(p[k].length){
			bits += length_extra[len_code(p[k].length) - 257];
			bits += distance_extra[dist_code(p[k].distance)];
		}
	}
	printf("predicted %lu bytes\n", (bits + 3 + 7) / 8);
#endif

	free(pairs);
	*deflate_input = w.buffer;
	return (long long) w.bwritten;

clean_on_failure:
	if(w.buffer) free(w.buffer);
	free(pairs);
	return -1;
}
