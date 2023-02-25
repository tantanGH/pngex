#include <string.h>
#include "himem.h"
#include "buffer.h"

//
//  open buffer
//
int32_t buffer_open(BUFFER_HANDLE* buf, FILE* fp) {

  buf->fp = fp;     // if fp is NULL, we use this instance as memory only buffer
  buf->rofs = 0;
  buf->wofs = 0;
//  buf->buffer_data = malloc_himem(buf->buffer_size, buf->use_high_memory);    // this works with 060turbo only
  buf->buffer_data = himem_malloc(buf->buffer_size, 0);

  return buf->buffer_data != NULL ? 0 : -1;
}

//
//  close buffer
//
void buffer_close(BUFFER_HANDLE* buf) {
  if (buf->buffer_data != NULL) {
//    free_himem(buf->buffer_data, buf->use_high_memory);
    himem_free(buf->buffer_data, 0);
  }
  // note: do not close fp
}

//
//  ring buffer operations (fill new data with file)
//
int32_t buffer_fill(BUFFER_HANDLE* buf, size_t len, int32_t return_to_top) {

  int32_t filled_size = 0;

  if (len < 0 ) {
    len = buf->buffer_size - buf->wofs;
  }

  if ((buf->wofs + len) <= buf->buffer_size) {
    // we can append all bytes to the buffer
    filled_size = fread(buf->buffer_data + buf->wofs, 1, len, buf->fp);
// no check
//    if (buf->wofs < buf->rofs && buf->wofs + filled_size > buf->rofs) {
//      filled_size = -1;   // unread data will be overwritten
//    } else {
      buf->wofs += filled_size;
//    }
  } else if (buf->wofs >= buf->buffer_size) {
    // we cannot append any bytes
    if (return_to_top) {
      // if return_to_top flas is yes, back to the top and fill
      filled_size = fread(buf->buffer_data + 0, 1, len, buf->fp);
      buf->wofs = filled_size;
      if (buf->rofs > 0 && buf->wofs > buf->rofs) {    // unread data were overwritten?
        filled_size = -1;
      }
    }
  } else {
    // we can append some bytes to the buffer
    int32_t available = buf->buffer_size - buf->wofs;
    filled_size = fread(buf->buffer_data + buf->wofs, 1, available, buf->fp);
    buf->wofs += filled_size;
    if (return_to_top) {
      // if return_to_top flas is yes, back to the top and fill      
      int32_t filled_size2 = fread(buf->buffer_data + 0, 1, len - available, buf->fp);
      filled_size += filled_size2;
      buf->wofs = filled_size2;
      if (buf->rofs > 0 && buf->wofs > buf->rofs) {    // unread data were overwritten?
        filled_size = -1;        
      }
    }
  }

  return filled_size;
}

//
//  ring buffer operations (read 1 byte)
//
uint8_t buffer_get_uchar(BUFFER_HANDLE* buf) {

  uint8_t uc;
  
  if (buf->rofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
     uc = buf->buffer_data[buf->rofs++];
  } else {
    // reset read offset
    uc = buf->buffer_data[0];
    buf->rofs = 1;
  }

  return uc;
}

//
//  ring buffer operations (read 2 bytes)
//
uint16_t buffer_get_ushort(BUFFER_HANDLE* buf, int32_t little_endian) {

  uint16_t us;

  if (buf->rofs < ( buf->buffer_size - 1)) {
    // we can read 2 bytes from the buffer
    // must not use bit or (|) here, not to discard upper bits
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[buf->rofs+1] << 8) : 
                         buf->buffer_data[buf->rofs+1] + (buf->buffer_data[buf->rofs] << 8); 
    buf->rofs += 2;
  } else if (buf->rofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[0] << 8) :
                         buf->buffer_data[0] + (buf->buffer_data[buf->rofs] << 8);
    buf->rofs = 1;
  } else {
    // we cannot read any bytes from the buffer
    us = little_endian ? buf->buffer_data[0] + (buf->buffer_data[1] << 8) :
                         buf->buffer_data[1] + (buf->buffer_data[0] << 8);
    buf->rofs = 2;
  }

  return us;
}

//
//  ring buffer operations (read 4 bytes)
//
uint32_t buffer_get_uint(BUFFER_HANDLE* buf, int32_t little_endian) {

  uint16_t us;

  if (buf->rofs < ( buf->buffer_size - 3)) {
    // we can read 4 bytes from the buffer
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[buf->rofs+1] << 8) + (buf->buffer_data[buf->rofs+2] << 16) + (buf->buffer_data[buf->rofs+3] << 24) :
                         buf->buffer_data[buf->rofs+3] + (buf->buffer_data[buf->rofs+2] << 8) + (buf->buffer_data[buf->rofs+1] << 16) + (buf->buffer_data[buf->rofs] << 24);
    buf->rofs += 4;
  } else if (buf->rofs < ( buf->buffer_size - 2)) {
    // we can read 3 bytes from the buffer
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[0] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[2] << 24) :
                         buf->buffer_data[2] + (buf->buffer_data[1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[buf->rofs] << 24);
    buf->rofs = 1;
  } else if (buf->rofs < ( buf->buffer_size - 1)) {
    // we can read 2 bytes from the buffer
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[buf->rofs+1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[1] << 24) :
                         buf->buffer_data[1] + (buf->buffer_data[0] << 8) + (buf->buffer_data[buf->rofs+1] << 16) + (buf->buffer_data[buf->rofs] << 24);
    buf->rofs = 2;
  } else if (buf->rofs < buf->buffer_size) {
    // we can read 1 byte from the buffer
    us = little_endian ? buf->buffer_data[buf->rofs] + (buf->buffer_data[0] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[2] << 24) :
                         buf->buffer_data[2] + (buf->buffer_data[1] << 8) + (buf->buffer_data[0] << 16) + (buf->buffer_data[buf->rofs] << 24);
    buf->rofs = 3;
  } else {
    // we cannot read any bytes from the buffer
    us = little_endian ? buf->buffer_data[0] + (buf->buffer_data[1] << 8) + (buf->buffer_data[2] << 16) + (buf->buffer_data[3] << 24) :
                         buf->buffer_data[3] + (buf->buffer_data[2] << 8) + (buf->buffer_data[1] << 16) + (buf->buffer_data[0] << 24);
    buf->rofs = 4;
  }

  return us;
}

//
//  ring buffer operations (read multiple bytes)
//
void buffer_read(BUFFER_HANDLE* buf, uint8_t* dest_ptr, size_t len) {

  if ((buf->rofs + len) <= buf->buffer_size) {
    // we can read all bytes from the buffer
    memcpy(dest_ptr, buf->buffer_data + buf->rofs, len);
    buf->rofs += len;
  } else if (buf->rofs >= buf->buffer_size) {
    // we cannot read any bytes from the buffer
    memcpy(dest_ptr, buf->buffer_data, len);
    buf->rofs = len;
  } else {
    // we can read some bytes from the buffer
    int available = buf->buffer_size - buf->rofs;
    memcpy(dest_ptr, buf->buffer_data + buf->rofs, available);
    memcpy(dest_ptr + available, buf->buffer_data, len - available);
    buf->rofs = len - available;
  }

}

//
//  ring buffer operations (write multiple bytes)
//
void buffer_write(BUFFER_HANDLE* buf, uint8_t* src_ptr, size_t len) {

  if ((buf->wofs + len) <= buf->buffer_size) {
    // we can write all bytes to the buffer
    memcpy(buf->buffer_data + buf->wofs, src_ptr, len);
    buf->wofs += len;
  } else if (buf->wofs >= buf->buffer_size) {
    // we cannot write any bytes to the buffer
    memcpy(buf->buffer_data, src_ptr, len);
    buf->wofs = len;
  } else {
    // we can read some bytes from the buffer
    int available = buf->buffer_size - buf->wofs;
    memcpy(buf->buffer_data + buf->wofs, src_ptr, available);
    memcpy(buf->buffer_data + 0, src_ptr + available, len - available);
    buf->wofs = len - available;
  }

}

//
//  ring buffer operations (just skip bytes)
//
void buffer_skip(BUFFER_HANDLE* buf, size_t len) {

  if ((buf->rofs + len) <= buf->buffer_size) {
    // we can skip all bytes from the buffer
    buf->rofs += len;
  } else if (buf->rofs >= buf->buffer_size) {
    // we cannot skip any bytes from the buffer
    buf->rofs = len;
  } else {
    // we can skip some bytes from the buffer
    int available = buf->buffer_size - buf->rofs;
    buf->rofs = len - available;
  }

}

//
//  discard and reset
//
void buffer_reset(BUFFER_HANDLE* buf) {
  buf->rofs = 0;
  buf->wofs = 0;
}