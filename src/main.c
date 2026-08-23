#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz77.h"

int main(){
	char i[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam a quam vehicula, tempor nisi ornare, fringilla lorem. Praesent faucibus sapien blandit ante dictum fermentum. Maecenas a accumsan tellus. Quisque eu nunc ante. Proin magna neque, pulvinar eu magna in, tristique auctor augue. Morbi quis lorem consectetur libero dapibus commodo. Maecenas facilisis sagittis mattis. Aenean consectetur dui tempus ante aliquet, vel condimentum leo pellentesque.\n Duis sed nibh eu magna porta scelerisque. Donec quis diam porta, tempor sapien non, convallis neque. Cras risus nunc, dapibus eget fermentum vitae, ultrices eu felis. Nunc non sapien egestas, laoreet velit volutpat, ultrices turpis. Praesent euismod convallis lacinia. Sed sit amet felis tempor lacus imperdiet pellentesque. Morbi fringilla aliquet pharetra.Pellentesque quis lectus sed purus euismod aliquet. Nullam interdum quis lacus sit amet sodales. Pellentesque sit amet lobortis mauris. Quisque ac semper dui. Phasellus ultrices sem at dolor tristique dictum. Proin scelerisque finibus quam, ultrices tempus felis dignissim in. Curabitur id luctus libero, in congue orci. Nam nec est sed nisi lobortis mollis. Praesent in ante tortor. Pellentesque efficitur, mauris in commodo gravida, enim sem posuere dui, at viverra orci lacus ac augue.Sed vel iaculis nibh. Sed facilisis vel dolor eu maximus. Praesent congue ipsum sit amet tempus fermentum. Nam imperdiet velit nec ligula vulputate, sit amet ultrices mi elementum. Etiam vitae nisi nec purus auctor malesuada vel eu lectus. Nam ultrices augue non arcu luctus posuere. Proin eu elit ex. Praesent scelerisque varius quam, quis tempus diam pretium eget. Aliquam commodo mi nec ipsum molestie malesuada. Cras vulputate eu lectus ut venenatis. Quisque egestas eget orci sit amet dictum. Vivamus maximus nulla ut elit interdum luctus non a quam. Sed vitae commodo nulla, vitae vulputate mauris.Vestibulum condimentum ullamcorper ipsum ac eleifend. Sed vel suscipit mi. Proin facilisis ultricies magna, in faucibus neque interdum eu. Nullam orci nisl, iaculis at mattis quis, mollis sed felis. Aliquam vel pellentesque purus, eget dignissim leo. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nunc fringilla, arcu eu condimentum vehicula, dui felis eleifend sapien, non porta elit metus vel urna.\nNulla vehicula justo ut purus ornare pretium et vitae ante. Fusce consectetur ac tellus eu placerat. Suspendisse semper auctor dui et cursus. In a massa ut enim mollis varius nec ut mi. Vestibulum sit amet erat nibh. Nulla facilisi. Etiam porta sodales mauris, a placerat leo. Nam eros ex, accumsan at erat vel, euismod maximus augue. Maecenas egestas ligula augue, eu rutrum nisi aliquet nec. Donec mollis volutpat vestibulum. Donec convallis urna mattis tortor aliquam convallis. Etiam luctus libero in pretium varius.";

	int input_size = strlen(i);
	uint8_t *i_bin = malloc(sizeof(uint64_t) +input_size);
	if(!i_bin)return -1;

	memset(i_bin,0,sizeof(uint64_t)+input_size);
	uint64_t i_s =(uint64_t) input_size;
	*(uint64_t*) i_bin = *(uint64_t*)&i_s;
	uint8_t *data = i_bin + 8;
	memcpy(data,i,input_size);

	size_t pair_size = input_size / sizeof(struct LDpair);
	pair_size *= sizeof(struct LDpair);

	uint64_t *pairs = (uint64_t*)malloc(sizeof(uint64_t) + (sizeof(struct LDpair) *pair_size));
	memset(pairs,0,sizeof(uint64_t) + ((sizeof(struct LDpair) * pair_size)));
	*pairs = pair_size;
	struct LDpair *p = (struct LDpair *)( pairs + 1);
	int tokens = LZ77_binary(data,&p);


	fprintf(stdout,"input was %d bytes long, created %d tokens:\n\n",(int)strlen(i),tokens);
	for(int i = 0; i < tokens;i++){
		fprintf(stdout,"{d = %d, l = %d, literal = '%u'}\n",p[i].distance,p[i].length,p[i].literal);
	}


	uint8_t decoded[input_size];
	memset(decoded,0,input_size);
	decode_LZ77(p,tokens,decoded);
	uint32_t lit_freq[286] = {0};
	uint32_t dist_freq[30] = {0};

	count_frequency(p, tokens,lit_freq,dist_freq);
	free(pairs);
	free(i_bin);

	struct Hnode n[MAX_LIT_TREE] = {0};
	struct Heap_literal h = {0};
	int16_t root = lit_tree(lit_freq,n,&h);
	printf("root is %d\n",root);
	if(memcmp(i,decoded,input_size) == 0){
		printf("inpunt and decoded are acutally equal.\ntest passed!\n");
	}
	return 0;
}
