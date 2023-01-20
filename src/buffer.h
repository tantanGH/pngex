#ifndef __H_BUILD__
#define __H_BUILD__

#include <stdio.h>
#include <stdint.h>

// ringer buffer handle
typedef struct {
  int32_t buffer_size;
  int32_t use_high_memory;
  FILE* fp;
  int32_t rofs;
  int32_t wofs;
  uint8_t* buffer_data;
} BUFFER_HANDLE;

// ring buffer operations
int32_t buffer_open(BUFFER_HANDLE* buf, FILE* fp);
void buffer_close(BUFFER_HANDLE* buf);
int32_t buffer_fill(BUFFER_HANDLE* buf, size_t len, int32_t return_to_top);
uint8_t buffer_get_uchar(BUFFER_HANDLE* buf);
uint16_t buffer_get_ushort(BUFFER_HANDLE* buf, int32_t little_endian);
uint32_t buffer_get_uint(BUFFER_HANDLE* buf, int32_t little_endian);
void buffer_read(BUFFER_HANDLE* buf, uint8_t* dest_ptr, size_t len);
void buffer_write(BUFFER_HANDLE* buf, uint8_t* dest_ptr, size_t len);
void buffer_skip(BUFFER_HANDLE* buf, size_t len);
void buffer_reset(BUFFER_HANDLE* buf);

#endif