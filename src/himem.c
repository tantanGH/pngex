#include <stdint.h>
#include <stddef.h>
#include <iocslib.h>
#include <doslib.h>
#include "himem.h"

// allocate high memory
static void* __himem_malloc(size_t size) {

    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 1;         // HIMEM_MALLOC
    in_regs.d2 = size;

    TRAP15(&in_regs, &out_regs);

    uint32_t rc = out_regs.d0;

    return (rc == 0) ? (void*)out_regs.a1 : NULL;
}

// free high memory
static void __himem_free(void* ptr) {
    
    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 2;         // HIMEM_FREE
    in_regs.d2 = (size_t)ptr;

    TRAP15(&in_regs, &out_regs);
}

// resize high memory
int __himem_resize(void* ptr, size_t size) {

    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;          // IOCS _HIMEM
    in_regs.d1 = 4;             // HIMEM_RESIZE
    in_regs.d2 = (size_t)ptr;
    in_regs.d3 = size;

    TRAP15(&in_regs, &out_regs);
  
    return out_regs.d0;
}

// allocate main memory
static void* __mainmem_malloc(size_t size) {
  uint32_t addr = MALLOC(size);
  return (addr >= 0x81000000) ? NULL : (void*)addr;
}

// free main memory
static void __mainmem_free(void* ptr) {
  if (ptr == NULL) return;
  MFREE((uint32_t)ptr);
}

// resize main memory
static int32_t __mainmem_resize(void* ptr, size_t size) {
  return SETBLOCK((uint32_t)ptr, size);
}

// allocate memory
void* himem_malloc(size_t size, int32_t use_high_memory) {
    return use_high_memory ? __himem_malloc(size) : __mainmem_malloc(size);
}

// free memory
void himem_free(void* ptr, int32_t use_high_memory) {
    if (use_high_memory) {
        __himem_free(ptr);
    } else {
        __mainmem_free(ptr);
    }
}

// resize memory
int32_t himem_resize(void* ptr, size_t size, int32_t use_high_memory) {
    return use_high_memory ? __himem_resize(ptr, size) : __mainmem_resize(ptr, size);
}

// check high memory availability
int32_t himem_isavailable() {
  int32_t v = B_LPEEK((uint32_t*)(0x000400 + 4 * 0xf8));   // check IOCS $F8 vector  
  return (v < 0 || (v >= 0xfe0000 && v <= 0xffffff)) ? 0 : 1;
}