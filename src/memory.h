#ifndef __H_MEMORY__
#define __H_MEMORY__

#include <stddef.h>
#include <stdint.h>

void* malloc_himem(size_t size, int32_t use_high_memory);
void free_himem(void* ptr, int32_t use_high_memory);

#endif