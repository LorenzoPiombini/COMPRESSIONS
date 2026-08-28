#include <wchar.h>
#include <stdio.h>
#include "os_operations.h"

#if defined(__linux__) || defined(__APPLE__)
	#include <sys/stat.h>
	#include <errno.h>
	#include <unistd.h>
	
	char parent_dir[1024] = {0};
#elif defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

int change_dir(char* dir_name)
{
#if defined(__linux__) || defined(__APPLE__)
	if(chdir(dir_name) == -1) return -1;
#elif defined(_WIN32) || defined(_WIN64)
#endif

	return 0;
}

int create_folder(char *file_name)
{

#if defined(__linux__) || defined(__APPLE__)
	if(!parent_dir[0]){
		errno = 0;
		if(getcwd(parent_dir,1024) == NULL){
			if(errno == EINVAL){
				fprintf(stderr,"bigger buffer for directory path");
			}
			return -1;
		}
	}

	errno = 0;
    if(mkdir(file_name, S_IRWXU | S_IFDIR ) == -1) {
		if(errno == EEXIST){
			errno = 0;
			if(chdir(file_name) == -1) return -1;
			return 0;
		}
		return -1;
	}

#elif defined(_WIN32) || defined(_WIN64)
	/*WINDOWS migth use wchar_t we need to convert the char * to wchar_t * */
	mbstate_t ps = 0;
	size_t l = strlen(file_name);
	wchar_t wstr[l+1];
	wmemset(wstr,0,l+1);

    if(mbsnrtowcs(wstr,file_name,l,l,&ps) == -1) return -1;

	DWORD attr = GetFileAttributesW(wstr);
	if(attr == INVALID_FILE_ATTRIBUTES){
		DWORD err = GetLastError();
		if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND){
			if(!SetCurrentDirectory(wstr)){
				return -1;
			}
			return 0;
		}
	}

	if(!CreateDirectoryW(wstr,NULL)){ 
			return -1;
	}

#endif
	return 0;
}
