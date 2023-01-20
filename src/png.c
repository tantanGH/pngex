#include <stdio.h>
#include <string.h>
#include <doslib.h>
#include <zlib.h>

#include "png.h"
#include "memory.h"
#include "buffer.h"

// GVRAM memory address
#define GVRAM       ((volatile uint16_t*)0xC00000)

//#define DEBUG

//
//  initialize PNG decode handle
//
void png_init(PNG_DECODE_HANDLE* png) {

  if (png->brightness == 0) {
    png->brightness = 100;
  }

  // actual width and height
  if (png->use_extended_graphic) {
    png->actual_width = 1024;
    png->actual_height = 1024;
  } else {
    png->actual_width = 512;
    png->actual_height = 512;
  }

  // for PNG decode
  png->current_x = -1;
  png->current_y = 0;
  png->current_filter = 0;

  png->left_rf = 0;
  png->left_gf = 0;
  png->left_bf = 0;

  png->up_rf_ptr = NULL;
  png->up_gf_ptr = NULL;
  png->up_bf_ptr = NULL;

  // allocate color map table memory
  png->rgb555_r = malloc_himem(512, png->use_high_memory);
  png->rgb555_g = malloc_himem(512, png->use_high_memory);
  png->rgb555_b = malloc_himem(512, png->use_high_memory);

  // initialize color map
  for (int32_t i = 0; i < 256; i++) {
    uint32_t c = (int)(i * 32 * png->brightness / 100) >> 8;
    png->rgb555_r[i] = (uint16_t)((c <<  6) + 1);
    png->rgb555_g[i] = (uint16_t)((c << 11) + 1);
    png->rgb555_b[i] = (uint16_t)((c <<  1) + 1);
  }

}

//
//  set PNG header (this can be done after we decode IHDR chunk)
//
void png_set_header(PNG_DECODE_HANDLE* png, PNG_HEADER* png_header) {

  // copy header content
  png->png_header.width              = png_header->width;
  png->png_header.height             = png_header->height;
  png->png_header.bit_depth          = png_header->bit_depth;
  png->png_header.color_type         = png_header->color_type;
  png->png_header.compression_method = png_header->compression_method;
  png->png_header.filter_method      = png_header->filter_method;
  png->png_header.interlace_method   = png_header->interlace_method;

  // allocate buffer memory for upper scanline filtering
  png->up_rf_ptr = malloc_himem(png_header->width, png->use_high_memory);
  png->up_gf_ptr = malloc_himem(png_header->width, png->use_high_memory);
  png->up_bf_ptr = malloc_himem(png_header->width, png->use_high_memory);

  // centering offset calculation
  if (png->centering) {
    int32_t screen_width  = png->use_extended_graphic ? 768 : 512;
    int32_t screen_height = 512;
    png->offset_x = ( png_header->width  <= screen_width ) ? ( screen_width  - png_header->width  ) >> 1 : 0;
    png->offset_y = ( png_header->height <= screen_width ) ? ( screen_height - png_header->height ) >> 1 : 0;
  }

}

//
//  release PNG decoder handle
//
void png_close(PNG_DECODE_HANDLE* png) {

  if (png == NULL) return;

  // reclaim color map memory
  if (png->rgb555_r != NULL) {
    free_himem(png->rgb555_r, png->use_high_memory);
    png->rgb555_r = NULL;
  }

  if (png->rgb555_g != NULL) {
    free_himem(png->rgb555_g, png->use_high_memory);
    png->rgb555_g = NULL;
  }

  if (png->rgb555_b != NULL) {
    free_himem(png->rgb555_b, png->use_high_memory);
    png->rgb555_b = NULL;
  }

  // reclaim filter buffer memory
  if (png->up_rf_ptr != NULL) {
    free_himem(png->up_rf_ptr, png->use_high_memory);
    png->up_rf_ptr = NULL;
  }

  if (png->up_gf_ptr != NULL) {
    free_himem(png->up_gf_ptr, png->use_high_memory);
    png->up_gf_ptr = NULL;
  }

  if (png->up_bf_ptr != NULL) {
    free_himem(png->up_bf_ptr, png->use_high_memory);
    png->up_bf_ptr = NULL;
  }

}

//
//  paeth predictor for PNG filter mode 4
//
inline static int16_t paeth_predictor(int16_t a, int16_t b, int16_t c) {
  int16_t p = a + b - c;
  int16_t pa = p > a ? p - a : a - p;
  int16_t pb = p > b ? p - b : b - p;
  int16_t pc = p > c ? p - c : c - p;
  if (pa <= pb && pa <= pc) {
    return a;  
  } else if (pb <= pc) {
    return b;
  }
  return c;
}

//
//  output pixel data to gvram
//
static void output_pixel(uint8_t* buffer, size_t buffer_size, int32_t* buffer_consumed, PNG_DECODE_HANDLE* png) {

  int32_t consumed_size = 0;
  int32_t bytes_per_pixel = (png->png_header.color_type == PNG_COLOR_TYPE_RGBA) ? 4 : 3;
  uint8_t* buffer_end = buffer + buffer_size;
  
  // cropping check
  if ((png->offset_y + png->current_y) >= png->actual_height) {
    // no need to output any pixels
    *buffer_consumed = buffer_size;     // just consumed all
    return;
  }

  // GVRAM entry point
  volatile uint16_t* gvram_current = GVRAM +  
                                    png->actual_width * (png->offset_y + png->current_y) + 
                                    png->offset_x + (png->current_x >= 0 ? png->current_x : 0);

  while (buffer < buffer_end) {

    if (png->current_x == -1) {    // first byte of each scan line

      // get filter mode
      png->current_filter = *buffer++;
#ifdef DEBUG
      //printf("g_current_filter=%d,g_current_y=%d\n",g_current_filter,g_current_y);
#endif
      // next pixel
      png->current_x++;

    } else {

      // before plotting, need to ensure we have accessible 3(or 4 bytes) in the inflated buffer
      // if not, we give up now and return
      if ((buffer_end - buffer) < bytes_per_pixel) {
        break;
      }

      // get raw RGB data
      int16_t r = *buffer++;
      int16_t g = *buffer++;
      int16_t b = *buffer++;

      // ignore 4th byte in RGBA mode
      if (png->png_header.color_type == PNG_COLOR_TYPE_RGBA) {
        buffer++;      
      }

      // filtered RGB
      int16_t rf, gf, bf;

      // apply filter
      switch (png->current_filter) {
      case 1:     // sub
        {
          int16_t arf = (png->current_x > 0) ? png->left_rf : 0;
          int16_t agf = (png->current_x > 0) ? png->left_gf : 0;
          int16_t abf = (png->current_x > 0) ? png->left_bf : 0;
          rf = ( r + arf ) & 0xff;
          gf = ( g + agf ) & 0xff;
          bf = ( b + abf ) & 0xff;
        }
        break;
      case 2:     // up
        {
          int16_t brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          int16_t bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          int16_t bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          rf = ( r + brf ) & 0xff;
          gf = ( g + bgf ) & 0xff;
          bf = ( b + bbf ) & 0xff;
        }
        break;
      case 3:     // average
        {
          int16_t arf = (png->current_x > 0) ? png->left_rf : 0;
          int16_t agf = (png->current_x > 0) ? png->left_gf : 0;
          int16_t abf = (png->current_x > 0) ? png->left_bf : 0;
          int16_t brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          int16_t bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          int16_t bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          rf = ( r + ((arf + brf) >> 1)) & 0xff;
          gf = ( g + ((agf + bgf) >> 1)) & 0xff;
          bf = ( b + ((abf + bbf) >> 1)) & 0xff;
        }
        break;
      case 4:     // paeth
        {
          int16_t arf = (png->current_x > 0) ? png->left_rf : 0;
          int16_t agf = (png->current_x > 0) ? png->left_gf : 0;
          int16_t abf = (png->current_x > 0) ? png->left_bf : 0;
          int16_t brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          int16_t bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          int16_t bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          int16_t crf = (png->current_x > 0 && png->current_y > 0) ? png->up_rf_ptr[png->current_x-1] : 0;
          int16_t cgf = (png->current_x > 0 && png->current_y > 0) ? png->up_gf_ptr[png->current_x-1] : 0;
          int16_t cbf = (png->current_x > 0 && png->current_y > 0) ? png->up_bf_ptr[png->current_x-1] : 0;
          rf = ( r + paeth_predictor(arf,brf,crf)) & 0xff;
          gf = ( g + paeth_predictor(agf,bgf,cgf)) & 0xff;
          bf = ( b + paeth_predictor(abf,bbf,cbf)) & 0xff;
        }
        break;
      default:    // none
        rf = r;
        gf = g;
        bf = b;
      }

      // write pixel data with cropping
      if ((png->offset_x + png->current_x) < png->actual_width) {
        *gvram_current++ = png->rgb555_r[rf] | png->rgb555_g[gf] | png->rgb555_b[bf];
      }
#ifdef DEBUG
      //printf("pixel: x=%d,y=%d,r=%d,g=%d,b=%d,rf=%d,gf=%d,bf=%d\n",g_current_x,g_current_y,r,g,b,rf,gf,bf);
#endif      
  
      // cache r,g,b for downstream filtering
      if (png->current_x > 0) {
        png->up_rf_ptr[ png->current_x -1 ] = png->left_rf;
        png->up_gf_ptr[ png->current_x -1 ] = png->left_gf;
        png->up_bf_ptr[ png->current_x -1 ] = png->left_bf;
      }
      if (png->current_x == png->png_header.width-1) {
        png->up_rf_ptr[ png->current_x ] = rf;
        png->up_gf_ptr[ png->current_x ] = gf;
        png->up_bf_ptr[ png->current_x ] = bf;
      }
      png->left_rf = rf;
      png->left_gf = gf;
      png->left_bf = bf;

      // next pixel
      png->current_x++;

      // next scan line
      if (png->current_x >= png->png_header.width) {
        png->current_x = -1;
        png->current_y++;
        if ((png->offset_y + png->current_y) >= png->actual_height) break;  // Y cropping
        gvram_current = GVRAM + png->actual_width * (png->offset_y + png->current_y) + png->offset_x;
      }

    }

  }

  *buffer_consumed = (buffer_size - (int32_t)(buffer_end - buffer));
}

//
//  inflate compressed data stream
//
static int32_t inflate_data(BUFFER_HANDLE* input_buffer, BUFFER_HANDLE* output_buffer, z_stream* zisp, PNG_DECODE_HANDLE* png) {

  int32_t z_status = Z_OK;

#ifdef DEBUG
  printf("input_buffer->rofs=%d,output_buffer->wofs=%d\n",input_buffer->rofs,output_buffer->wofs);
#endif

  zisp->next_in = input_buffer->buffer_data + input_buffer->rofs;
  zisp->avail_in = input_buffer->buffer_size - input_buffer->rofs;
  if (zisp->next_out == Z_NULL) {
    zisp->next_out = output_buffer->buffer_data + output_buffer->wofs;
    zisp->avail_out = output_buffer->buffer_size - output_buffer->wofs;
  }

#ifdef DEBUG
    printf("z_status=%d,avail_in=%d,avail_out=%d\n",z_status,zisp->avail_in,zisp->avail_out);
#endif

  while (zisp->avail_in > 0) {

    int32_t avail_in_cur = zisp->avail_in;
    int32_t avail_out_cur = zisp->avail_out;

    // inflate
    z_status = inflate(zisp,Z_NO_FLUSH);
#ifdef DEBUG
    printf("inflated. z_status=%d,avail_in_cur=%d,avail_in=%d,avail_out_cur=%d,avail_out=%d,wofs=%d\n",z_status,avail_in_cur,zisp->avail_in,avail_out_cur,zisp->avail_out,output_buffer->wofs);
#endif
    if (z_status == Z_OK) {

      // input buffer consumed
      int32_t in_consumed_size = avail_in_cur - zisp->avail_in;
      input_buffer->rofs += in_consumed_size;
      if (input_buffer->rofs >= input_buffer->buffer_size) {
#ifdef DEBUG        
    printf("input buffer fully consumed. z_status=%d,avail_in=%d,avail_out=%d\n",z_status,zisp->avail_in,zisp->avail_out);
#endif
        input_buffer->rofs = 0;     // but not yet refilled
      }

      // output buffer consumed
      int32_t inflated_size = avail_out_cur - zisp->avail_out;
      output_buffer->wofs += inflated_size;  

      // output pixel
#ifdef DEBUG
      printf("output_buffer->rofs=%d\n",output_buffer->rofs);
#endif
      int32_t out_consumable_size = output_buffer->wofs - output_buffer->rofs;
      int32_t out_consumed_size;
      output_pixel(output_buffer->buffer_data + output_buffer->rofs, out_consumable_size, &out_consumed_size, png);

      // in case we cannot consume all the inflated data, reuse it for the next output
      int32_t out_remain_size = out_consumable_size - out_consumed_size;
#ifdef DEBUG        
    printf("output pixel done. z_status=%d,avail_in=%d,avail_out=%d,out_consumed=%d,remain=%d\n",z_status,zisp->avail_in,zisp->avail_out,out_consumed_size,out_remain_size);
#endif
      if (out_remain_size > 0) {
#ifdef DEBUG        
    printf("output buffer remained. z_status=%d,avail_in=%d,avail_out=%d,inflated=%d,out_consumed=%d,out_remain=%d\n",z_status,zisp->avail_in,zisp->avail_out,inflated_size,out_consumed_size,out_remain_size);
#endif
        memcpy(output_buffer->buffer_data, output_buffer->buffer_data + out_consumed_size, out_remain_size);
        output_buffer->wofs = out_remain_size;
        output_buffer->rofs = 0;
      } else {
        output_buffer->wofs = 0;
        output_buffer->rofs = 0;
      }

      // for next inflate operation
      zisp->next_in = input_buffer->buffer_data + input_buffer->rofs;
      zisp->next_out = output_buffer->buffer_data + output_buffer->wofs;
      zisp->avail_out = output_buffer->buffer_size - output_buffer->wofs;

    } else if (z_status == Z_STREAM_END) {

      // output buffer written
      int32_t inflated_size = avail_out_cur - zisp->avail_out;
      output_buffer->wofs += inflated_size;  

      // input buffer consumed
      input_buffer->rofs += avail_in_cur - zisp->avail_in;
      if (input_buffer->rofs >= input_buffer->buffer_size) {
        input_buffer->rofs = 0;
      }

      // output pixel
#ifdef DEBUG
      printf("output_buffer->rofs=%d\n",output_buffer->rofs);
#endif
      int32_t out_consumable_size = output_buffer->wofs - output_buffer->rofs;
      int32_t out_consumed_size;
      output_pixel(output_buffer->buffer_data + output_buffer->rofs, out_consumable_size, &out_consumed_size, png);

      // in case we cannot consume all the inflated data, reuse it for the next output
      int32_t out_remain_size = out_consumable_size - out_consumed_size;
      if (out_remain_size > 0) {
        memcpy(output_buffer->buffer_data, output_buffer->buffer_data + out_consumed_size, out_remain_size);
        output_buffer->wofs = out_remain_size;
        output_buffer->rofs = 0;
      } else {
        output_buffer->wofs = 0;
        output_buffer->rofs = 0;
      }

      break;

    } else {
      //printf("error: data inflation error(%d).\n",z_status);
      break;
    }
  }

  return z_status;
}

//
//  load PNG image
//
int32_t png_load(PNG_DECODE_HANDLE* png, const uint8_t* png_file_name) {

  // return code
  int32_t rc = -1;

  // for file operation
  FILE* fp;
  uint8_t signature[8];

  // png header
  PNG_HEADER png_header;

  // input buffer
  BUFFER_HANDLE input_buffer = { 0 };

  // output buffer
  BUFFER_HANDLE output_buffer = { 0 };

  // for zlib inflate operation  
  z_stream zis;
  zis.zalloc = Z_NULL;
  zis.zfree = Z_NULL;
  zis.opaque = Z_NULL;
  zis.avail_in = 0;
  zis.next_in = Z_NULL;
  zis.avail_out = 0;
  zis.next_out = Z_NULL;

  // initialize zlib
  if (inflateInit(&zis) != Z_OK) {
    printf("error: zlib inflate initialization error.\n");
    goto catch;
  }

  // open source file
  fp = fopen(png_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open input file (%s).\n", png_file_name);
    goto catch;
  }

  // instantiate input buffer
  input_buffer.buffer_size = png->input_buffer_size;
  if (buffer_open(&input_buffer, fp) != 0) {
    printf("error: input buffer initialization error.\n");
    goto catch;
  }

  // fill the buffer for signature
  if (buffer_fill(&input_buffer, 8, 0) < 8) {
    printf("error: file is too small to check signature. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }

  // check signature
  buffer_read(&input_buffer, signature, 8);
  if (!png->no_signature_check && memcmp(signature,"\x89PNG\r\n\x1a\n",8) != 0 ) {
    printf("error: signature error. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }

  // instantiate output buffer
  output_buffer.buffer_size = png->output_buffer_size;
  if (buffer_open(&output_buffer, NULL) != 0) {
    printf("error: output buffer initialization error.\n");
    goto catch;
  }

  // process PNG file chunk by chunk
  for (;;) {

    int32_t chunk_size, chunk_crc;
    uint8_t chunk_type[5];
  
    // get chunk size from file (not buffer)
    //int chunk_size = buffer_get_uint(&input_buffer, 0);
    fread((void*)&chunk_size, 1, 4, fp);

    // get chunk type from file (not buffer)
    //buffer_copy(&input_buffer, chunk_type, 4);
    fread(chunk_type, 1, 4, fp);
    chunk_type[4] = '\0';

#ifdef DEBUG
    printf("chunk_type = [%s], chunk_size = [%d], rofs = [%d], wofs = [%d]\n", chunk_type, chunk_size, input_buffer.rofs, input_buffer.wofs);
#endif

    if (strcmp("IHDR",chunk_type) == 0) {

      // IHDR - header chunk, we can assume this chunk appears at top

      // read chunk data and crc into input buffer
      buffer_fill(&input_buffer, chunk_size + 4, 0);

      // parse header
      png_header.width              = buffer_get_uint(&input_buffer, 0);
      png_header.height             = buffer_get_uint(&input_buffer, 0);
      png_header.bit_depth          = buffer_get_uchar(&input_buffer);
      png_header.color_type         = buffer_get_uchar(&input_buffer);
      png_header.compression_method = buffer_get_uchar(&input_buffer);
      png_header.filter_method      = buffer_get_uchar(&input_buffer);
      png_header.interlace_method   = buffer_get_uchar(&input_buffer);

      // check bit depth (support 8bit color only)
      if (png_header.bit_depth != 8) {
        printf("error: unsupported bit depth (%d).\n",png_header.bit_depth);
        goto catch;
      }

      // check color type (support RGB or RGBA only)
      if (png_header.color_type != 2 && png_header.color_type != 6) {
        printf("error: unsupported color type (%d).\n",png_header.color_type);
        goto catch;
      }

      // check interlace mode
      if (png_header.interlace_method != 0) {
        printf("error: interlace png is not supported.\n");
        goto catch;
      }

      // set header to handle
      png_set_header(png, &png_header);

      // reset buffer
      //buffer_skip(&input_buffer, (chunk_size - 13) + 4);
      buffer_reset(&input_buffer);

    } else if (strcmp("IDAT",chunk_type) == 0) {

      // IDAT - data chunk, may appear several times

      // read chunk data into input buffer
      int32_t filled_size = buffer_fill(&input_buffer, chunk_size, 0);
      if (filled_size < 0) {
        printf("error: buffer error. unread data were overwritten.\n");
        goto catch;
      } else if (filled_size < chunk_size) {
#ifdef DEBUG
        printf("reached to buffer end. cannot read all the data. (filled=%d,chunk_size=%d,rofs=%d,wofs=%d)\n",filled_size,chunk_size,input_buffer.rofs,input_buffer.wofs);
#endif
        // consume data here
        int32_t z_status = inflate_data(&input_buffer, &output_buffer, &zis, png);
        if (z_status != Z_OK && z_status != Z_STREAM_END) {
          printf("error: zlib data decompression error(%d).\n",z_status);
          goto catch;
        }

        // back to buffer top and refill
        int32_t refilled_size = buffer_fill(&input_buffer, chunk_size - filled_size, 1);
        if (refilled_size < 0) {
          printf("error: buffer error. unread data were overwritten.\n");
          goto catch;          
        }
#ifdef DEBUG
        printf("refilled %d bytes. (rofs=%d,wofs=%d)\n",refilled_size,input_buffer.rofs,input_buffer.wofs);
#endif        
      }

      // read crc from file (not from buffer)
      fread((uint8_t*)(&chunk_crc), 1, 4, fp);

      // no crc check

    } else if (strcmp("IEND",chunk_type) == 0) {

      // IEND chunk - the very last chunk
      break;

    } else {

      // unknown chunk - just skip
      fseek(fp, chunk_size + 4, SEEK_CUR);

    }

  }

  // do we have any unconsumed data?
  if (input_buffer.rofs != input_buffer.wofs) {
    // consume data here
    int z_status = inflate_data(&input_buffer,&output_buffer,&zis,png);
    if (z_status != Z_OK && z_status != Z_STREAM_END) {
      printf("error: zlib data decompression error(%d).\n",z_status);
      goto catch;
    }
  }

  // complete zlib inflation stream operation
  inflateEnd(&zis);

  // succeeded
  rc = 0;

catch:
  // close source PNG file
  fclose(fp);
  
  // close input buffer
  buffer_close(&input_buffer);

  // close output buffer
  buffer_close(&output_buffer);
  
  // done
  return rc;
}

//
//  describe PNG file information
//
int32_t png_describe(PNG_DECODE_HANDLE* png, const uint8_t* png_file_name) {

  // return code
  int32_t rc = -1;

  // for file operation
  FILE* fp;
  uint8_t signature[8];

  // png header
  PNG_HEADER png_header;

  // input buffer
  BUFFER_HANDLE input_buffer = { 0 };

  // open source file
  fp = fopen(png_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open input file (%s).\n", png_file_name);
    goto catch;
  }

  // instantiate input buffer
  input_buffer.buffer_size = png->input_buffer_size;
  if (buffer_open(&input_buffer, fp) != 0) {
    printf("error: input buffer initialization error.\n");
    goto catch;
  }

  // fill the buffer as much as possible
  if (buffer_fill(&input_buffer, -1, 0) < 8) {
    printf("error: file is too small to check signature. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }

  // check signature
  buffer_read(&input_buffer, signature, 8);
  if (!png->no_signature_check && memcmp(signature,"\x89PNG\r\n\x1a\n",8) != 0) {
    printf("error: signature error. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }
  
  // process PNG file chunk by chunk
  for (;;) {
 
    uint8_t chunk_type[5];
 
    // get chunk size from buffer
    int32_t chunk_size = buffer_get_uint(&input_buffer, 0);

    // read next 4 bytes as chuck type
    buffer_read(&input_buffer, (uint8_t*)&chunk_type, 4);
    chunk_type[4] = '\0';

    if (strcmp("IHDR",chunk_type) == 0) {

      // IHDR - header chunk, we can assume this chunk appears at top
      struct FILBUF inf;

      // parse header
      png_header.width              = buffer_get_uint(&input_buffer, 0);
      png_header.height             = buffer_get_uint(&input_buffer, 0);
      png_header.bit_depth          = buffer_get_uchar(&input_buffer);
      png_header.color_type         = buffer_get_uchar(&input_buffer);
      png_header.compression_method = buffer_get_uchar(&input_buffer);
      png_header.filter_method      = buffer_get_uchar(&input_buffer);
      png_header.interlace_method   = buffer_get_uchar(&input_buffer);

      FILES(&inf,(uint8_t*)png_file_name,0x23);
      printf("--\n");
      printf(" file name: %s\n",png_file_name);
      printf(" file size: %d\n",inf.filelen);
      printf(" file time: %04d-%02d-%02d %02d:%02d:%02d\n",1980+(inf.date>>9),(inf.date>>5)&0xf,inf.date&0x1f,inf.time>>11,(inf.time>>5)&0x3f,inf.time&0x1f);
      printf("     width: %d\n",png_header.width);
      printf("    height: %d\n",png_header.height);
      printf(" bit depth: %d\n",png_header.bit_depth);
      printf("color type: %d\n",png_header.color_type);
      printf(" interlace: %d\n",png_header.interlace_method);

      rc = 0;
      break;

    } else {

      // skip other chunks
      buffer_skip(&input_buffer, chunk_size + 4);

    }

  }

catch:
  // close buffer
  buffer_close(&input_buffer);

  // close source PNG file
  fclose(fp);

  // done
  return rc;
}
