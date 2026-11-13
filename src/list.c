#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "list.h"


#define T List_T

T List_push(T list, void *x)
{
    T p = malloc(sizeof(T*));
    if (!p) return NULL;
    memset(p,0,sizeof(T));

    p->first = x;
    p->rest = list;
    return p;
}

T List_list(void *x, ...)
{
    va_list ap;
    T list, *p = &list;

    va_start(ap,x);
    for(; x; x = va_arg(ap,void *)){
        *p = malloc(sizeof(T));
        if(!(*p)){
            va_end(ap);
            return NULL;
        }
        (*p)->first = x;
        p = &(*p)->rest;
    }

    *p = NULL;
    va_end(ap);
    return list;
}

T List_append(T list, T tail)
{
    T *p = &list;
    
    while(*p)
        p = &(*p)->rest;
    *p = tail;
    return list;
}

T List_pop(T list, void **x)
{
    if (list) {
        T head = list->rest;
        if (x)
            *x = list->first;
        free(list);
        return head;
    }

    return list;

}
