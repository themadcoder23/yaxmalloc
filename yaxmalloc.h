#ifndef YAXMALLOC_H
#define YAXMALLOC_H
#include <stddef.h>
void mem_init_sandbox();
void *mm_malloc(size_t size);
void mm_free(void *ptr);
int mm_init();
#endif