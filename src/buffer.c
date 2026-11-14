#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "buffer.h"




static int strip_nt(char *str,size_t s);
/*HTML parser helpers*/
static int HTML_clear_white_space(char *str,size_t s);
static void HTML_comment(line_T *line);
static int HTML_tag_open(char *line,size_t s);
static int HTML_tag_close(char *line,size_t s);


// buf is the buffer to extract the line from
// buf_s is the buffer size 
// lc is the line extracted 
// return the size of each line
int get_line(char *buf,size_t buf_s, line_T* lc)
{
    if(!buf) return EOF;
    if(*buf == '\0') return EOF;
    if(buf_s == 0) return EOF;


    /*get the line from buf*/
    char *p = buf;
    while((p - buf) < buf_s && *p != '\n') p++;
    
    
    int line_len = p - buf;   
    /*include '\n' if found*/
    if(p - buf < buf_s) line_len++;

    lc->start = buf;
    lc->len = line_len;
    return 0;
}

static int strip_nt(char *str,size_t s)
{
    size_t j = 0;
    for(size_t i = 0; i < s;i++){
        switch(str[i]){
        case '\n': case '\t': case '\r': str[i] = '\0';break;
        default: 
        }
    }   
    compact_str(str,s);
    return 0;
}

void compact_str(char *str, size_t s)
{
    for(size_t i = 0; i < s; i++) {
        if((s - i) > 1 && str[i] == '\0'){
            size_t j = i;
            while( j < s && str[j] == '\0') j++;

            /*the rest of the line is '\0' break out*/
            if(j == s) break;

            int len = s - j;
            int gap_to_fil = j - i;
            int place_holder = (j + len) -1;
            memmove(&str[i],&str[j],len);
            /*zerod the gap*/
            while(gap_to_fil > 0) {
                str[place_holder] = '\0';
                gap_to_fil--;
                place_holder--;
            }
        }
    }
}
static void HTML_comment(line_T *line)
{
    char *comment_start, *comment_end;
    comment_end = comment_start = NULL;
    size_t i = 0; 
    size_t lo = strlen("<!--");
    size_t lc = strlen("-->");
    while (i < line->len) {
        if(memcmp(&line->start[i], "<!--", lo) == 0) {
            comment_start = &line->start[i];
            i += lo;
            break;
        }
        i += lo;
    }

    if(!comment_start) return;

    while (i < line->len) {
        if(memcmp(&line->start[i], "-->", lc) == 0) {
            comment_end = &line->start[i];
            i += lc;
            break;
        }
        i += lc;
    }

    if(!comment_end) return;

    /*account for --> size*/
    comment_end += 3; 
    int comment_size = (int)((comment_end - line->start) - (comment_start - line->start));
    memset(comment_start,0,comment_size);
}

/*check if the line is a tag*/
static int HTML_tag_open(char *line,size_t s)
{
    for(size_t i = 0; i < s; i++) {
        if (line[i] == '<') return 1;
    }
    return 0;
}

static int HTML_tag_close(char *line, size_t s)
{
    for(size_t i = 0; i < s; i++) {
        if (line[i] == '>') return 1;
    }
    return 0;
}

int HTML_parse_line(line_T *line)
{
    HTML_clear_white_space(line->start,line->len);
    HTML_comment(line);
    
    /*full opening tag and in one line*/
    if(HTML_tag_open(line->start,line->len) && HTML_tag_close(line->start,line->len)){
        strip_nt(line->start,line->len);
        return 0;
    }

    /*tag is in more than one line*/
    if(HTML_tag_open(line->start,line->len) && !HTML_tag_close(line->start,line->len)){
        strip_nt(line->start,line->len);
        return 0;
    }
    
    /*closin openingin/closing tag*/
    if(!HTML_tag_open(line->start,line->len) && HTML_tag_close(line->start,line->len)){
        strip_nt(line->start,line->len);
        return 0;
    }
    /*a regular line*/
    strip_nt(line->start,line->len);
    return 0;
}

Lines *line_push(Lines *l, line_T **el)
{
    Lines *ls = malloc(sizeof *l); 
    if(!ls) return NULL;
    memset(ls,0,sizeof *l);
    
    ls->first = *el;
    ls->next = l;
    return ls;
}

void line_free(Lines *l)
{
    Lines *next;
    for(;l; l = next){
        next = l->next;
        free(l->first->start);
        free(l->first);
        free(l);
    }
}

static int HTML_clear_white_space(char *str,size_t s)
{
# define bound_check ((s - i) > 1)
    int string,p_tag,open_tag,closing_tag;
    p_tag = string = open_tag = closing_tag = 0;

    for(size_t i = 0; i < s; i++){
        switch (str[i]) {
        case '<':
            if(bound_check && string == 0){
                if(str[i+1] == '/'){ /*a tag is closing*/
                    if(p_tag) p_tag = 0; 
                    if(open_tag) open_tag = 0;
                }else{
                    open_tag = 1;
                    if(str[i+1] == 'p') p_tag = open_tag;
                }
            }
            break;     
        case '\"': 
            if(string) 
                string = 0;
            else
                string = 1;
            break;
        case ' ':
            if(string) break;
            if(p_tag) break;
            if(closing_tag){
                if(str[i+1] == '>'){
                    str[i] = '\0';
                    closing_tag = 0;
                }
            }
            if(open_tag){
                if(bound_check
                    && (str[i+1] == '='
                        || str[i+1] == '\"'
                        || str[i+1] == '/'
                        || str[i+1] == '>')){
                    str[i] = '\0';
                    if(str[i+1] == '/'){
                        open_tag = 0;
                        closing_tag = 1;
                    }
              
                    break;
               }
            }
            if(bound_check
                && (str[i+1] == ' '
                    || str[i+1] == '<'
                    || str[i+1] == '/'
                    || str[i+1] == '>')){
                str[i] = '\0';
            }
            break;
        default: break;
        }
    }
#undef bound_check
    compact_str(str,s);
}
