#ifndef _OS_OPERATIONS_H
#define _OS_OPERATIONS_H

#if defined(__linux__) || defined(__APPLE__)
	#define PATH_OS "/"
#elif defined(_WIN32) || defined(_WIN64)
	#define PATH_OS "\\"
#endif

int create_folder(char *file_name);
int change_dir(char* dir_name);

#endif
