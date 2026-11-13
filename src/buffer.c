#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "buffer.h"



uint64_t line = 0;

static int strip_nt(uint64_t *buf);
static void compact_str(char *str, size_t size);
static int HTML_clear_white_space(char *str);
static int HTML_comment(char *line, char **p);
static int HTML_clear_white_space(char *str);
static int HTML_tag_open(char *line);
static int HTML_tag_close(char *line);

#define MASK_ADDR 0x0000FFFFFFFFFFFF
#define MASK_SIZE 0xFFFF000000000000
#define GET_SIZE(n) ((((uint64_t)n) & MASK_SIZE) >> 48)
#define SET_SIZE(a,n) (a) = (a) | (((uint64_t)n) << 48)

int buf_handle_init()
{
    char *l = malloc(ALLOC_LINE_BUF);
    if(!l) return -1;      
    memset(l,0,ALLOC_LINE_BUF);
    
    line = (uint64_t)l;
    return 0;    
}

void buf_close()
{
    free((char *)(line & 0x0000FFFFFFFFFFFF));
}

// buf is the buffer to extract the line from
// lc is the line extracted 
// return the size of each line
int get_line(char *buf, char** lc)
{
    if(!buf) return EOF;
    if(*buf == '\0') return EOF;

    char *l = (char *)(line & MASK_ADDR);  
    /*if the address is not initialize, do it!*/
    if(!l) buf_handle_init();    

    l = (char *)(line & MASK_ADDR);
    memset(l,0,ALLOC_LINE_BUF);

    /*get the line from buf*/
    char *end = strchr(buf,'\n');
    if(!end) return EOF;
    
    int line_len = end - buf;   
    /*include '\n'*/
    line_len++;
    strncpy(l,buf,line_len);
    *lc = l;
    SET_SIZE(line,line_len);
    return line_len;
}

static int strip_nt(uint64_t *buf)
{
    size_t s = (size_t)((*buf & 0xFFFF000000000000) >> 48);
    uint8_t* origin = (uint8_t*)(*buf & 0x0000FFFFFFFFFFFF);
    size_t j = 0;
    for(size_t i = 0; i < s;i++){
        switch(origin[i]){
        case '\n': case '\t': case '\r': origin[i] = '\0';break;
        default: 
        }
    }   
    compact_str(origin,s);
    return 0;
}

static void compact_str(char *str, size_t s)
{

    for(size_t i = 0; i < s; i++) {
        if((s - i) > 1 && str[i] == '\0'){
            size_t j = i;
            while( j < s && str[j] == '\0') j++;

            /*the rest of the line is '\0' break out*/
            if(j == s) break;

            int len = strlen(&str[j]);
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
static int HTML_comment(char *line, char **p)
{
    char *comment_start = strstr(line,"<!--");
    if(!comment_start) return -1;
    char *comment_end = strstr(line,"-->");
    if(!comment_end) return -1;

    /*account for --> size*/
    comment_end += 3; 
    *p = comment_start;
    return (int)((comment_end - line) - (comment_start - line));
}

/*check if the line is a tag*/
static int HTML_tag_open(char *line)
{
   return strstr(line,"<") ? 1 : 0;
}

static int HTML_tag_close(char *line)
{
   return strstr(line,">") ? 1 : 0;
}

int HTML_parse_line(char *line)
{

    size_t line_l  = strlen(line);
    char *p = NULL;
    HTML_clear_white_space(line);
    int comment_l = 0;
    if((comment_l = HTML_comment(line,&p)) > 0){
        /*erese the coment from the line*/
        memset(p,0,comment_l);
        char *s = p;
        /*consolidate the line*/
        int size_left = line_l - ((p+comment_l)-line); 
        memcpy(s,p + comment_l,size_left); 
    }
    
    /*full opening tag and in one line*/
    if(HTML_tag_open(line) && HTML_tag_close(line)){
        uint64_t l = (uint64_t)line;
        l = l | ((uint64_t)line_l << 48);
        strip_nt(&l);
        return 0;
    }

    /*tag is in more than one line*/
    if(HTML_tag_open(line) && !HTML_tag_close(line)){
        uint64_t l = (uint64_t)line;
        l = l | ((uint64_t)line_l << 48);
        strip_nt(&l);
        return 0;
    }
    
    /*closin openingin/closing tag*/
    if(!HTML_tag_open(line) && HTML_tag_close(line)){
        uint64_t l = (uint64_t)line;
        l = l | ((uint64_t)line_l << 48);
        strip_nt(&l);
        return 0;
    }
    /*a regular line*/
    uint64_t l = (uint64_t)line;
    l = l | ((uint64_t)line_l << 48);
    strip_nt(&l);
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
        free(l->first);
        free(l);
    }
}

static int HTML_clear_white_space(char *str)
{
# define bound_check ((s - i) > 1)
    size_t s = strlen(str);
    int string,p_tag,open_tag;
    p_tag = string = open_tag = 0;

    for(size_t i = 0; i < s; i++){
        switch (str[i]) {
        case '<':
            if(bound_check && string == 0){
                if(str[i+1] == '/'){ /*a tag is closing*/
                    if(p_tag) p_tag = 0; 
                    if(open_tag) open_tag = 0;
                }else{
                    open_tag = 1;
                    if(str[i] == 'p') p_tag = open_tag;
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
            if(open_tag){
                if(bound_check
                    && (str[i+1] == '='
                        || str[i+1] == '\"'
                        || str[i+1] == '/')){
                    str[i] = '\0';
                    if(str[i+1] == '/' && open_tag) open_tag = 0;
                    break;
               }
            }
            if(bound_check
                && (str[i+1] == ' '
                    || str[i+1] == '<' )){
                str[i] = '\0';
            }
            break;
        default: break;
        }
    }
#undef bound_check
    compact_str(str,s);
}
