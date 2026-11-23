#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 


/*HTML parser flags*/
#define MULT_LINE_COMM          1U
#define P_TAG_OPEN              2U
#define PRE_TAG_OPEN            4U
#define SCRIPT_TAG_OPEN         8U
#define NESTED_TAG_OPEN         16U
#define PRE_TAG_ONE_LINE        32U
#define SCRIPT_TAG_ONE_LINE     64U
#define MULTI_P_TAG_OPEN        128U
#define MULTI_PRE_TAG_OPEN      256U
#define OPEN_TAG                512U

#define IS_MULT_LINE_COMM(n)        (((n) & MULT_LINE_COMM) != 0)  
#define IS_P_TAG_OPEN(n)            (((n) & P_TAG_OPEN)  != 0)
#define IS_PRE_TAG_OPEN(n)          (((n) & PRE_TAG_OPEN) != 0)
#define IS_SCRIPT_TAG_OPEN(n)       (((n) & SCRIPT_TAG_OPEN) != 0)
#define IS_NESTED_TAG_OPEN(n)       (((n) & NESTED_TAG_OPEN) != 0)
#define IS_PRE_TAG_ONE_LINE(n)      (((n) & PRE_TAG_ONE_LINE) != 0)
#define IS_SCRIPT_TAG_ONE_LINE(n)   (((n) & SCRIPT_TAG_ONE_LINE) != 0)
#define IS_MULTI_P_TAG_OPEN(n)      (((n) & MULTI_P_TAG_OPEN)  != 0)
#define IS_MULTI_PRE_TAG_OPEN(n)    (((n) & MULTI_PRE_TAG_OPEN)  != 0)
#define IS_OPEN_TAG(n)              (((n) & OPEN_TAG) != 0)

#define SET_MULT_LINE_COMM(n)       ((n) |= MULT_LINE_COMM)
#define SET_P_TAG_OPEN(n)           ((n) |= P_TAG_OPEN)
#define SET_PRE_TAG_OPEN(n)         ((n) |= PRE_TAG_OPEN)
#define SET_SCRIPT_TAG_OPEN(n)      ((n) |= SCRIPT_TAG_OPEN)
#define SET_NESTED_TAG_OPEN(n)      ((n) |= NESTED_TAG_OPEN)
#define SET_SCRIPT_TAG_ONE_LINE(n)  ((n) |= SCRIPT_TAG_ONE_LINE)
#define SET_PRE_TAG_ONE_LINE(n)     ((n) |= PRE_TAG_ONE_LINE)
#define SET_MULTI_P_TAG_OPEN(n)     ((n) |= MULTI_P_TAG_OPEN)
#define SET_MULTI_PRE_TAG_OPEN(n)   ((n) |= MULTI_PRE_TAG_OPEN)
#define SET_OPEN_TAG(n)             ((n) |= OPEN_TAG)


#define UNSET_MULT_LINE_COMM(n)         ((n) &= ~MULT_LINE_COMM)
#define UNSET_P_TAG_OPEN(n)             ((n) &= ~P_TAG_OPEN)
#define UNSET_PRE_TAG_OPEN(n)           ((n) &= ~PRE_TAG_OPEN)
#define UNSET_SCRIPT_TAG_OPEN(n)        ((n) &= ~SCRIPT_TAG_OPEN)
#define UNSET_NESTED_TAG_OPEN(n)        ((n) &= ~NESTED_TAG_OPEN)
#define UNSET_SCRIPT_TAG_ONE_LINE(n)    ((n) &= ~SCRIPT_TAG_ONE_LINE)
#define UNSET_PRE_TAG_ONE_LINE(n)       ((n) &= ~PRE_TAG_ONE_LINE)
#define UNSET_MULTI_PRE_TAG_OPEN(n)     ((n) &= ~MULTI_PRE_TAG_OPEN)
#define UNSET_MULTI_P_TAG_OPEN(n)       ((n) &= ~MULTI_P_TAG_OPEN)
#define UNSET_OPEN_TAG(n)               ((n) &= ~OPEN_TAG)


enum mode{
    TEXT,
    TAG,
    COMMENT,
    SCRIPT,
    STYLE,
    PRE,
    TEXTAREA
};

typedef struct parser{
    enum mode state;
    char quote;
    int pending_space;
}parser;

typedef struct line line_T;

struct line{
    char *start;
    size_t len;
};

int get_line(char *buf,size_t buf_s, line_T* lc);
int HTML_parse_line(line_T *line,uint32_t *flags);
void compact_str(char *str, size_t size);
long long find_byte0(uint8_t* buf, size_t s);
uint8_t *HTML_minimize(uint8_t *file_content, size_t *s);

#endif
