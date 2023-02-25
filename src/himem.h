#ifndef __H_HIMEM__
#define __H_HIMEM__

#include <stdint.h>
#include <stddef.h>

void* himem_malloc(size_t size, int32_t use_high_memory);
void himem_free(void* ptr, int32_t use_high_memory);
int32_t himem_resize(void* ptr, size_t size, int32_t use_high_memory);
int32_t himem_isavailable(void);

#endif