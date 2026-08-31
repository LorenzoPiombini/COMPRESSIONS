#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz77.h"
#include "os_operations.h"

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


	if(F_Gzip("lorem.txt") == -1) return -1;

	fprintf(stdout,"'%s' compressed!\n","lorem.txt");
	return 0;

	
	/*test_GZIP(argv[1]);*/
	if(argv[1])
		test_ZIP(argv[1]);
	return 0;
}
