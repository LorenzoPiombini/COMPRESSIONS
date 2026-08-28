#include <sys/stat.h>
#include <wchar.h>
#include "os_operations.h"


int create_folder(char *file_name)
{

#if defined(__linux__) || defined(__APPLE__)
    if(mkdir(file_name, S_IRWXU | S_IFDIR ) == -1) return -1;
#elif defined(_WIN32) || defined(_WIN64)
	/*WINDOWS migth uses wchar_t we need to convert the char * to wchar_t * */
	mbstate_t ps = 0;
	size_t l = strlen(file_name);
	wchar_t wstr[l+1];
	wmemset(wstr,0,l+1);

    if(mbsnrtowcs(wstr,file_name,l,l,&ps) == -1) return -1;
	if(!CreateDirectoryW(wstr,NULL) return -1;

#endif

	return 0;

}
