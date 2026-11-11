#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stdint.h> 

extern uint64_t line;

#define ALLOC_LINE_BUF  (uint16_t)0xFFFF
#define LIMIT           (uint16_t)0xFFFF-1

int buf_handle_init();
int get_line(char *buf, char** lc);
int HTML_parse_line(char *line);
void buf_close();

#endif
