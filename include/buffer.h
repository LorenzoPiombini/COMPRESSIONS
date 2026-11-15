#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 


/*HTML parser flags*/
#define MULT_LINE_COMM  0x00000001U
#define P_TAG_OPEN      0x00000002U
#define PRE_TAG_OPEN    0x00000004U
#define SCRIPT_TAG_OPEN 0x00000008U
#define NESTED_TAG_OPEN 0x00000010U

#define IS_MULT_LINE_COMM(n)    (((n) & MULT_LINE_COMM) != 0)  
#define IS_P_TAG_OPEN(n)        (((n) & P_TAG_OPEN)  != 0)
#define IS_PRE_TAG_OPEN(n)      (((n) & PRE_TAG_OPEN) != 0)
#define IS_SCRIPT_TAG_OPEN(n)   (((n) & SCRIPT_TAG_OPEN) != 0)
#define IS_NESTED_TAG_OPEN(n)   (((n) & NESTED_TAG_OPEN) != 0)

#define SET_MULT_LINE_COMM(n)   ((n) |= MULT_LINE_COMM)
#define SET_P_TAG_OPEN(n)       ((n) |= P_TAG_OPEN)
#define SET_PRE_TAG_OPEN(n)     ((n) |= PRE_TAG_OPEN)
#define SET_SCRIPT_TAG_OPEN(n)  ((n) |= SCRIPT_TAG_OPEN)
#define SET_NESTED_TAG_OPEN(n)  ((n) |= NESTED_TAG_OPEN)

#define UNSET_MULT_LINE_COMM(n)     ((n) &= ~MULT_LINE_COMM)
#define UNSET_P_TAG_OPEN(n)         ((n) &= ~P_TAG_OPEN)
#define UNSET_PRE_TAG_OPEN(n)       ((n) &= ~PRE_TAG_OPEN)
#define UNSET_SCRIPT_TAG_OPEN(n)    ((n) &= ~SCRIPT_TAG_OPEN)
#define UNSET_NESTED_TAG_OPEN(n)    ((n) &= ~NESTED_TAG_OPEN)

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
int HTML_parse_line(line_T *line,uint32_t *flags);
Lines *line_push(Lines *l, line_T **el);
void line_free(Lines *l);
void compact_str(char *str, size_t size);
long long find_byte0(uint8_t* buf, size_t s);

#endif
