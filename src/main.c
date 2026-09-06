#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include "lz77.h"
#include "os_operations.h"

static int test_ZIP(char *argv){

	char d[250] = {0};
	struct F_unzip data  = {0};
	char *dir = strstr(argv,".");
	if(!dir){
		/*create a directory for the extraction*/
		int l = (int)strlen(argv);
		if(l > 250) return -1;

		d[0] = 'd';
		d[1] = '.';
		int i = 2, j = 0;
		while(j < l) d[i++] = argv[j++];
		if(mkdir(d, S_IRWXU | S_IFDIR ) == -1) return -1;
	}else{
		int stop = dir - argv; 
		d[0] = 'd';
		d[1] = '.';
		int i = 2, j = 0;
		while(j < stop) d[i++] = argv[j++];
		if(mkdir(d, S_IRWXU | S_IFDIR ) == -1) return -1;
	}

	uint8_t *file_content = NULL; 
	long long size = 0;
	if((size = read_file(argv,&file_content)) == -1) return -1;

	if(change_dir(d) == -1) return -1;

	if(unZIP(file_content,size,&data) == -1) goto failed;

	if(write_extracted_ZIP(&data) == -1) goto failed;

	free(file_content);
	free(data.data);
	return 0;

failed:
	if(file_content) free(file_content);
	if(data.data) free(data.data);
	return -1;
}

int main(int argc, char **argv){

	uint8_t *p = NULL;
	if(F_Gzip("lorem.txt",-1,&p) == -1) return -1;

	fprintf(stdout,"'%s' compressed!\n","lorem.txt");

	if(F_Gzip("lorem.txt",M_FILE,&p) == -1) return -1;
	
	free(p);
	/*test_GZIP(argv[1]);*/
	if(argc < 2){
		printf("you gotta specify a file to zip!");
		return -1;
	}

	if(argv[1])
		test_ZIP(argv[1]);
	return 0;
}
