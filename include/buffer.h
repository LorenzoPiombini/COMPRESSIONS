#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 

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

int get_line(char *buf,size_t buf_s, line_T* lc);
int HTML_parse_line(line_T *line,int *mlc);
Lines *line_push(Lines *l, line_T **el);
void line_free(Lines *l);
void compact_str(char *str, size_t size);
long long find_byte0(uint8_t* buf, size_t s);

#endif
