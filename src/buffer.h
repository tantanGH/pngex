#ifndef __H_BUILD__
#define __H_BUILD__

#include <stdio.h>

// ringer buffer handle
typedef struct {
  int buffer_size;
  int use_high_memory;
  FILE* fp;
  int rofs;
  int wofs;
  unsigned char* buffer_data;
} BUFFER_HANDLE;

// ring buffer operations
int buffer_open(BUFFER_HANDLE* buf, FILE* fp);
void buffer_close(BUFFER_HANDLE* buf);
int buffer_fill(BUFFER_HANDLE* buf, int len, int return_to_top);
unsigned char buffer_get_uchar(BUFFER_HANDLE* buf);
unsigned short buffer_get_ushort(BUFFER_HANDLE* buf, int little_endian);
unsigned int buffer_get_uint(BUFFER_HANDLE* buf, int little_endian);
void buffer_read(BUFFER_HANDLE* buf, unsigned char* dest_ptr, int len);
void buffer_write(BUFFER_HANDLE* buf, unsigned char* dest_ptr, int len);
void buffer_skip(BUFFER_HANDLE* buf, int len);
void buffer_reset(BUFFER_HANDLE* buf);

#endif