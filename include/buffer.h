#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 
#include "list.h"

extern uint64_t line;

#define ALLOC_LINE_BUF  (uint16_t)0xFFFF
#define LIMIT           (uint16_t)0xFFFF-1

typedef struct file_buffer buf_T;
typedef struct line line_T;

struct line{
    char *start;
    size_t len;
};

struct file_buffer{
    long line_nr;
    List_T current_line;  
    size_t size; 
};


int buf_handle_init();
int get_line(char *buf, char** lc);
int HTML_parse_line(char *line);
void buf_close();

#endif
