#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "buffer.h"



static int strip_nt(char *str,size_t s,uint32_t *flags);
/*HTML parser helpers*/
static int HTML_clear_white_space(char *str,size_t s);
static int HTML_check_tag(char *line,size_t s,uint32_t *flags);
static void HTML_comment(line_T *line,uint32_t *flags);


// buf is the buffer to extract the line from
// buf_s is the buffer size 
// lc is the line extracted 
// return the size of each line
int get_line(char *buf,size_t buf_s, line_T* lc)
{
    if(!buf) return EOF;
    if(buf_s == 0) return EOF;
    if(*buf == '\0') return EOF;


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

static int strip_nt(char *str,size_t s,uint32_t *flags)
{
    size_t j = 0;
    for(size_t i = 0; i < s;i++){
        
        switch(str[i]){
        case '\n': case '\t': case '\r': 
            if(IS_PRE_TAG_OPEN(*flags))break;
            if(IS_OPEN_TAG(*flags)){
                str[i] = ' ';break;
            }else{
                str[i] = '\0';break;
            }
            
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
static void HTML_comment(line_T *line,uint32_t *flags)
{
    char *comment_start, *comment_end;
    comment_end = comment_start = NULL;
    size_t i = 0; 
    size_t lo = strlen("<!--");
    size_t lc = strlen("-->");
    while(i <= line->len){
        if(IS_MULT_LINE_COMM(*flags) == 0){
            while (i + lo <= line->len) {
                if(memcmp(&line->start[i], "<!--", lo) == 0) {
                    comment_start = &line->start[i];
                    i += lo;
                    break;
                }
                i++;
            }

            if(!comment_start) return;
        }

        while (i + lc <= line->len) {
            if(memcmp(&line->start[i], "-->", lc) == 0) {
                comment_end = &line->start[i];
                i += lc;
                break;
            }
            i++;
        }

        if(!comment_end){
            /*multi comment line*/
            /*zeroed all line*/
            if(IS_MULT_LINE_COMM(*flags)){
                memset(line->start,0,line->len);
            }else if (comment_start) {
                SET_MULT_LINE_COMM(*flags);
                memset(comment_start,0,line->len -(comment_start - line->start));
            }
            return;
        }

        if(IS_MULT_LINE_COMM(*flags)) {
            UNSET_MULT_LINE_COMM(*flags);
        }
        /*account for --> size*/
        comment_end += 3; 
        if(!comment_start){
            int comment_size = (int)(comment_end - line->start);
            memset(line->start,0,comment_size);
        }else{
            int comment_size = (int)((comment_end - line->start) - (comment_start - line->start));
            memset(comment_start,0,comment_size);
        }
        comment_start = NULL;
        comment_end = NULL;
   } 
}

/*check if the line is a tag*/
static int HTML_check_tag(char *line,size_t s,uint32_t *flags)
{
#define bound_check(n) (s - i > (n))
    int was_opened_scr = 0, was_opened_pre = 0;
    for(size_t i = 0; i < s; i++) {
        if (line[i] == '<' 
            && bound_check(1)
            && line[i+1] != '/'){
            SET_OPEN_TAG(*flags);
            if(IS_PRE_TAG_OPEN(*flags) 
                || IS_P_TAG_OPEN(*flags) 
                || IS_MULTI_P_TAG_OPEN(*flags)
                || IS_SCRIPT_TAG_OPEN(*flags) 
                || IS_MULTI_PRE_TAG_OPEN(*flags)){
                SET_NESTED_TAG_OPEN(*flags);
            }
            if( bound_check(3) 
                && line[i+1] == 'p' 
                && line[i+2] != 'r'){
                SET_P_TAG_OPEN(*flags);
            } 
            if( bound_check(4) 
                && line[i+1] == 'p' 
                && line[i+2] == 'r'
                && line[i+3] == 'e'){
                SET_PRE_TAG_OPEN(*flags);
                was_opened_pre = 1;
            } 
            if( bound_check(7) 
                && line[i+1] == 's' 
                && line[i+2] == 'c'
                && line[i+3] == 'r'
                && line[i+4] == 'i'
                && line[i+5] == 'p'
                && line[i+6] == 't'){
                SET_SCRIPT_TAG_OPEN(*flags);
                was_opened_scr = 1;
            } 
        } 
            if( bound_check(1)
                && (line[i] == '<' 
                    && line[i+1] == '/')){
                    if(bound_check(7)
                            && line[i+2] == 's' && line[i+3] == 'c'
                            && line[i+4] == 'r' && line[i+5] == 'i'
                            && line[i+6] == 'p' && line[i+7] == 't'){
                        if(IS_SCRIPT_TAG_OPEN(*flags))      UNSET_SCRIPT_TAG_OPEN(*flags);
                        if(IS_MULTI_PRE_TAG_OPEN(*flags))   UNSET_MULTI_PRE_TAG_OPEN(*flags);
                        if(IS_NESTED_TAG_OPEN(*flags))      UNSET_NESTED_TAG_OPEN(*flags);
                    }
                    if(bound_check(5)
                        && line[i+2] == 'p'
                        && line[i+3] == 'r'
                        && line[i+4] == 'e'){
                        if(IS_PRE_TAG_OPEN(*flags))         UNSET_PRE_TAG_OPEN(*flags);
                        if(IS_MULTI_PRE_TAG_OPEN(*flags))   UNSET_MULTI_PRE_TAG_OPEN(*flags);
                        if(IS_NESTED_TAG_OPEN(*flags))      UNSET_NESTED_TAG_OPEN(*flags);
                    }
                    if(bound_check(4) 
                        && line[i+2] == 'p'
                        && line[i+3] != 'e'){
                        if(IS_MULTI_P_TAG_OPEN(*flags))     UNSET_MULTI_P_TAG_OPEN(*flags);
                        if(IS_P_TAG_OPEN(*flags))           UNSET_P_TAG_OPEN(*flags);
                    }
            }

            if(bound_check(2) 
                && ((line[i] == '<'
                    && line[i+1] == '/')
                        || (line[i] == '/'
                            && line[i+1] == '>'))) UNSET_OPEN_TAG(*flags);
    }
   
    
    if(was_opened_scr
        && !IS_SCRIPT_TAG_OPEN(*flags)) SET_SCRIPT_TAG_ONE_LINE(*flags);
    if(was_opened_pre
        && !IS_PRE_TAG_OPEN(*flags)) SET_PRE_TAG_ONE_LINE(*flags);
#undef bound_check
    return 0;
}

/*flags may cointain 32 flag*/
int HTML_parse_line(line_T *line,uint32_t *flags)
{
    HTML_comment(line,flags);
    if(IS_MULT_LINE_COMM(*flags)) return 0;

    if(IS_PRE_TAG_OPEN(*flags) 
        || IS_SCRIPT_TAG_OPEN(*flags)
        || IS_MULTI_P_TAG_OPEN(*flags)
        || IS_MULTI_PRE_TAG_OPEN(*flags)){
        HTML_check_tag(line->start,line->len,flags);
        return 0;
    }

    
   
    HTML_check_tag(line->start,line->len,flags);
    if(IS_P_TAG_OPEN(*flags) || IS_PRE_TAG_OPEN(*flags)) {
        strip_nt(line->start,line->len,flags);
        return 0;
    }

    if(IS_PRE_TAG_ONE_LINE(*flags)){
        UNSET_PRE_TAG_ONE_LINE(*flags);
        return 0;
    }

    if(IS_SCRIPT_TAG_ONE_LINE(*flags)){
        UNSET_SCRIPT_TAG_ONE_LINE(*flags);
        return 0;
    }

    strip_nt(line->start,line->len,flags);

    HTML_clear_white_space(line->start,line->len);
    return 0;
}

static int HTML_clear_white_space(char *str,size_t s)
{
# define bound_check ((s - i) > 1)
    int string, was_s,was_d,p_tag,open_tag,closing_tag;
    p_tag = string =was_d=was_s= open_tag = closing_tag = 0;

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
            case '\'':
                if((string && was_s) && str[i] == '\"') break;
                if((string && was_d) && str[i] == '\"') {
                    string = 0;
                    was_d = 0;
                    break;
                }
                if((string && was_d) && str[i] == '\'') break;
                if((string && was_s) && str[i] == '\'') {
                    string = 0;
                    was_s = 0;
                    break;
                }
                if(string)
                    string = 0;
                else{
                    if(str[i] == '\"') was_d = 1;
                    if(str[i] == '\'') was_s = 1;
                    string = 1;
                }
                break;
            case ' ':
                if(string) break;
                if(open_tag || p_tag) break;
                if(closing_tag){
                    if(bound_check && str[i+1] == '>'){
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

long long find_byte0(uint8_t *buf,size_t s)
{
    for(size_t i = 0; i < s; i++)
        if(buf[i] == '\0') return (long long) i; 
        
    return -1;
}


/*THIS WAS AI SUGGESTED*/
int check_state(char *buf,size_t s, parser *p)
{
#define BOUND_CHECK(n) ((n) < s)
    if(!buf) return -1;
    switch(*buf){
    case '<':
    {
        if ( BOUND_CHECK(4) 
             && buf[1] == '!'
             && buf[2] == '-'
             && buf[3] == '-') p->state = COMMENT;
        break;
    }
    case '-':
    {
        if ( BOUND_CHECK(3) 
             && buf[1] == '-'
             && buf[2] == '>') p->state = COMMENT;
    }
    }
    return 0;
}

uint8_t *HTML_minimize(uint8_t *file_content, size_t *s)
{
    line_T lc = {0};
    uint8_t *start = file_content;
    int mlc = 0;
    while(file_content - start < *s 
            && get_line((char *)file_content,
                *s - (file_content - start),&lc) != EOF){

        HTML_parse_line(&lc,&mlc);
        file_content += lc.len;
    }

    compact_str(start,*s);
    long long ns = 0;
    if((ns = find_byte0(start,*s)) != -1) *s = (size_t) ns;
    return start;
}
