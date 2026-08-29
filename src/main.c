#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz77.h"
#include "os_operations.h"

static int test_GZIP(char *argv){
	uint8_t *file_content = NULL; 
	FILE *fp = fopen(argv,"rb");
	if(!fp)return -1;

	if(fseek(fp,0,SEEK_END) == -1) goto failed;

	long long size = 0;
	if((size = ftell(fp)) == -1) goto failed;

	rewind(fp);

	file_content = malloc(size);
	if(!file_content) goto failed;
	if(fread(file_content,(size_t)size,1,fp) != 1) goto failed;

	fclose(fp);
	fp = NULL;
	uint8_t *inflated_outup = NULL;
	uint64_t inflated_outup_size = 0;
	if(inflate_GZIP(file_content,size, &inflated_outup, &inflated_outup_size) == -1) goto failed;

	free(file_content);
	return 0;

failed:
	if(inflated_outup) free(inflated_outup);
	if(fp) fclose(fp);
	if(file_content) free(file_content);
	return -1;
}

static int test_ZIP(char *argv){

	char d[250] = {0};
	char *dir = strstr(argv,".");
	if(!dir){
		/*create a directory for the extraction*/
		size_t l = strlen(argv);
		if(l > 250) return -1;

		d[0] = 'd';
		d[1] = '.';
		int i = 2, j = 0;
		while(j < l) d[i++] = argv[j++];
		if(create_folder(d) == -1) return -1;
	}else{
		int stop = dir - argv; 
		d[0] = 'd';
		d[1] = '.';
		int i = 2, j = 0;
		while(j < stop) d[i++] = argv[j++];
		if(create_folder(d) == -1) return -1;
	}

	uint8_t *file_content = NULL; 
	FILE *fp = fopen(argv,"rb");
	if(!fp)return -1;

	if(fseek(fp,0,SEEK_END) == -1) goto failed;

	long long size = 0;
	if((size = ftell(fp)) == -1) goto failed;

	rewind(fp);

	file_content = malloc(size);
	if(!file_content) goto failed;
	if(fread(file_content,(size_t)size,1,fp) != 1) goto failed;

	fclose(fp);
	fp = NULL;
	if(change_dir(d) == -1) return -1;

	if(unZIP(file_content,size) == -1) goto failed;

	free(file_content);
	return 0;

failed:
	if(fp) fclose(fp);
	if(file_content) free(file_content);
	return -1;
}

int main(int argc, char **argv){
	char i[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam a quam vehicula, tempor nisi ornare, fringilla lorem. Praesent faucibus sapien blandit ante dictum fermentum. Maecenas a accumsan tellus. Quisque eu nunc ante. Proin magna neque, pulvinar eu magna in, tristique auctor augue. Morbi quis lorem consectetur libero dapibus commodo. Maecenas facilisis sagittis mattis. Aenean consectetur dui tempus ante aliquet, vel condimentum leo pellentesque.\n Duis sed nibh eu magna porta scelerisque. Donec quis diam porta, tempor sapien non, convallis neque. Cras risus nunc, dapibus eget fermentum vitae, ultrices eu felis. Nunc non sapien egestas, laoreet velit volutpat, ultrices turpis. Praesent euismod convallis lacinia. Sed sit amet felis tempor lacus imperdiet pellentesque. Morbi fringilla aliquet pharetra.Pellentesque quis lectus sed purus euismod aliquet. Nullam interdum quis lacus sit amet sodales. Pellentesque sit amet lobortis mauris. Quisque ac semper dui. Phasellus ultrices sem at dolor tristique dictum. Proin scelerisque finibus quam, ultrices tempus felis dignissim in. Curabitur id luctus libero, in congue orci. Nam nec est sed nisi lobortis mollis. Praesent in ante tortor. Pellentesque efficitur, mauris in commodo gravida, enim sem posuere dui, at viverra orci lacus ac augue.Sed vel iaculis nibh. Sed facilisis vel dolor eu maximus. Praesent congue ipsum sit amet tempus fermentum. Nam imperdiet velit nec ligula vulputate, sit amet ultrices mi elementum. Etiam vitae nisi nec purus auctor malesuada vel eu lectus. Nam ultrices augue non arcu luctus posuere. Proin eu elit ex. Praesent scelerisque varius quam, quis tempus diam pretium eget. Aliquam commodo mi nec ipsum molestie malesuada. Cras vulputate eu lectus ut venenatis. Quisque egestas eget orci sit amet dictum. Vivamus maximus nulla ut elit interdum luctus non a quam. Sed vitae commodo nulla, vitae vulputate mauris.Vestibulum condimentum ullamcorper ipsum ac eleifend. Sed vel suscipit mi. Proin facilisis ultricies magna, in faucibus neque interdum eu. Nullam orci nisl, iaculis at mattis quis, mollis sed felis. Aliquam vel pellentesque purus, eget dignissim leo. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nunc fringilla, arcu eu condimentum vehicula, dui felis eleifend sapien, non porta elit metus vel urna.\nNulla vehicula justo ut purus ornare pretium et vitae ante. Fusce consectetur ac tellus eu placerat. Suspendisse semper auctor dui et cursus. In a massa ut enim mollis varius nec ut mi. Vestibulum sit amet erat nibh. Nulla facilisi. Etiam porta sodales mauris, a placerat leo. Nam eros ex, accumsan at erat vel, euismod maximus augue. Maecenas egestas ligula augue, eu rutrum nisi aliquet nec. Donec mollis volutpat vestibulum. Donec convallis urna mattis tortor aliquam convallis. Etiam luctus libero in pretium varius.";

	int input_size = strlen(i);
	uint8_t *i_bin = malloc(sizeof(uint64_t) +input_size);
	if(!i_bin)return -1;

	memset(i_bin,0,sizeof(uint64_t)+input_size);
	uint64_t i_s =(uint64_t) input_size;
	*(uint64_t*) i_bin = *(uint64_t*)&i_s;
	uint8_t *data = i_bin + 8;
	memcpy(data,i,input_size);


	uint8_t *df_in = NULL;
	long long r = 0;
	if((r = deflate(data,input_size,&df_in)) == -1){
		free(i_bin);
		if(df_in) free(df_in);
		return -1;
	}

	/*TEST!! write the compressed data to file*/
	FILE *f = fopen("out.gz", "wb");
	uint8_t hdr[10] = {0x1f, 0x8b, 0x08, 0, 0,0,0,0, 0, 0xff};
	fwrite(hdr, 1, 10, f);
	fwrite(df_in, 1, r, f);

	crc32_init();
	uint32_t crc = crc32(data,input_size);
	uint32_t isize = 2930;
	fwrite(&crc, 4, 1, f);      /* little-endian */
	fwrite(&isize, 4, 1, f);
	fclose(f);
	free(i_bin);
	free(df_in);

	
	/*test_GZIP(argv[1]);*/
	if(argv[1])
		test_ZIP(argv[1]);
	return 0;
}
