#include "buffer.h"
#include "memory.h"

//
//  open buffer
//
int buffer_open(BUFFER_HANDLE* buf, FILE* fp) {

  int rc = -1;

  buf->fp = fp;
  buf->ofs = 0;

  buf->buffer_data = malloc_himem(buf->buffer_size, buf->use_high_memory);
  if (buf->buffer_data != NULL) {
    fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    rc = 0;
  }

  return rc;
}

//
//  close buffer
//
void buffer_close(BUFFER_HANDLE* buf) {
  if (buf->buffer_data != NULL) {
    free_himem(buf->buffer_data, buf->use_high_memory);
  }
  // note: do not use fp
}

//
//  ring buffer operations (read 1 byte)
//
unsigned char buffer_get_uchar(BUFFER_HANDLE* buf) {

  unsigned char uc;
  
  if (buf->ofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
     uc = buf->buffer_data[buf->ofs++];
  } else {
    // we cannot read any bytes from the buffer -> read more data from file
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    uc = buf->buffer_data[0];
    buf->ofs = 1;
  }

  return uc;
}

//
//  ring buffer operations (read 2 bytes)
//
unsigned short buffer_get_ushort(BUFFER_HANDLE* buf, int little_endian) {

  unsigned short us;

  if (buf->ofs < ( buf->buffer_size - 1)) {
    // we can read 2 bytes from the buffer
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[buf->ofs+1] << 8) :    // must not use bit or (|) here, not to discard upper bits
                         buf->buffer_data[buf->ofs+1] + (buf->buffer_data[buf->ofs] << 8); 
    buf->ofs += 2;
  } else if (buf->ofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size - 1, buf->fp);      // fill buffer from file
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[0] << 8) :   // must not use bit or (|) here, not to discard upper bits
                         buf->buffer_data[0] + (buf->buffer_data[buf->ofs] << 8);
    buf->ofs = 1;
  } else {
    // we cannot read any bytes from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    us = little_endian ? buf->buffer_data[0] + (buf->buffer_data[1] << 8) :          // must not use bit or (|) here, not to discard upper bits
                         buf->buffer_data[1] + (buf->buffer_data[0] << 8);
    buf->ofs = 2;
  }

  return us;
}

//
//  ring buffer operations (read 4 bytes)
//
unsigned int buffer_get_uint(BUFFER_HANDLE* buf, int little_endian) {

  unsigned short us;

  if (buf->ofs < ( buf->buffer_size - 3)) {
    // we can read 4 bytes from the buffer
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[buf->ofs+1] << 8) + (buf->buffer_data[buf->ofs+2] << 16) + (buf->buffer_data[buf->ofs+3] << 24) :
                         buf->buffer_data[buf->ofs+3] + (buf->buffer_data[buf->ofs+2] << 8) + (buf->buffer_data[buf->ofs+1] << 16) + (buf->buffer_data[buf->ofs] << 24);
    buf->ofs += 4;
  } else if (buf->ofs < ( buf->buffer_size - 2)) {
    // we can read 3 bytes from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size - 3, buf->fp);
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[0] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[2] << 24) :
                         buf->buffer_data[2] + (buf->buffer_data[1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[buf->ofs] << 24);
    buf->ofs = 1;
  } else if (buf->ofs < ( buf->buffer_size - 1)) {
    // we can read 2 bytes from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size - 2, buf->fp);
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[buf->ofs+1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[1] << 24) :
                         buf->buffer_data[1] + (buf->buffer_data[0] << 8) + (buf->buffer_data[buf->ofs+1] << 16) + (buf->buffer_data[buf->ofs] << 24);
    buf->ofs = 2;
  } else if (buf->ofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size - 1, buf->fp);
    us = little_endian ? buf->buffer_data[buf->ofs] + (buf->buffer_data[0] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[2] << 24) :
                         buf->buffer_data[2] + (buf->buffer_data[1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[buf->ofs] << 24);
    buf->ofs = 3;
  } else {
    // we cannot read any bytes from the buffer
    int read_size = fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    us = little_endian ? buf->buffer_data[0] + (buf->buffer_data[1] << 8) + (buf->buffer_data[2] << 16) + (buf->buffer_data[3] << 24) :
                         buf->buffer_data[3] + (buf->buffer_data[2] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[0] << 24);
    buf->ofs = 4;
  }

  return us;
}

//
//  ring buffer operations (copy multiple bytes)
//
void buffer_copy(BUFFER_HANDLE* buf, unsigned char* dest_ptr, int len) {

  if ((buf->ofs + len) <= buf->buffer_size) {
    // we can read all bytes from the buffer
    memcpy(dest_ptr, buf->buffer_data + buf->ofs, len);
    buf->ofs += len;
  } else if (buf->ofs >= buf->buffer_size) {
    // we cannot read any bytes from the buffer
    fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    memcpy(dest_ptr, buf->buffer_data, len);
    buf->ofs = len;
  } else {
    // we can read some bytes from the buffer
    int available = buf->buffer_size - buf->ofs;
    memcpy(dest_ptr, buf->buffer_data + buf->ofs, available);
    fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    memcpy(dest_ptr + available, buf->buffer_data, len - available);
    buf->ofs = len - available;
  }

}

//
//  ring buffer operations (just skip bytes)
//
void buffer_skip(BUFFER_HANDLE* buf, int len) {

  if ((buf->ofs + len) <= buf->buffer_size) {
    // we can skip all bytes from the buffer
    buf->ofs += len;
  } else if (buf->ofs >= buf->buffer_size) {
    // we cannot skip any bytes from the buffer
    fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    buf->ofs = len;
  } else {
    // we can skip some bytes from the buffer
    int available = buf->buffer_size - buf->ofs;
    fread(buf->buffer_data, 1, buf->buffer_size, buf->fp);
    buf->ofs = len - available;
  }

}