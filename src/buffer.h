#ifndef __H_BUILD__
#define __H_BUILD__

#include <stdio.h>
#include <string.h>

// ringer buffer handle
typedef struct {
  int buffer_size;
  int use_high_memory;
  FILE* fp;
  int ofs;
  unsigned char* buffer_data;
} BUFFER_HANDLE;

// ring buffer operations
int buffer_open(BUFFER_HANDLE* buf, FILE* fp);
void buffer_close(BUFFER_HANDLE* buf);
unsigned char buffer_get_uchar(BUFFER_HANDLE* buf);
unsigned short buffer_get_ushort(BUFFER_HANDLE* buf, int little_endian);
unsigned int buffer_get_uint(BUFFER_HANDLE* buf, int little_endian);
void buffer_copy(BUFFER_HANDLE* buf, unsigned char* dest_ptr, int len);
void buffer_skip(BUFFER_HANDLE* buf, int len);

#endif