#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 

extern uint64_t line;

#define ALLOC_LINE_BUF  (uint16_t)0xFFFF
#define LIMIT           (uint16_t)0xFFFF-1

typedef struct file_buffer buf_T;
typedef struct line line_T;
typedef struct lines Lines;

struct line{
    char *start;
    size_t len;
};

struct lines{
    line_T *first;
    Lines *next;
};

struct file_buffer{
    long line_nr;
    Lines *ls;  
    size_t size; 
};

int buf_handle_init();
int get_line(char *buf, char** lc);
int HTML_parse_line(char *line);
Lines *line_push(Lines *l, line_T **el);
void line_free(Lines *l);
void buf_close();

#endif
