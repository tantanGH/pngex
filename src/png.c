#include "png.h"
#include "memory.h"

#include <doslib.h>
#include <zlib.h>

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
  for (int i = 0; i < 256; i++) {
    unsigned int c = (int)(i * 32 * png->brightness / 100) >> 8;
    png->rgb555_r[i] = (unsigned short)((c <<  6) + 1);
    png->rgb555_g[i] = (unsigned short)((c << 11) + 1);
    png->rgb555_b[i] = (unsigned short)((c <<  1) + 1);
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
    int screen_width  = png->use_extended_graphic ? 768 : 512;
    int screen_height = 512;
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
inline static short paeth_predictor(short a, short b, short c) {
  short p = a + b - c;
  short pa = p > a ? p - a : a - p;
  short pb = p > b ? p - b : b - p;
  short pc = p > c ? p - c : c - p;
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
static void output_pixel(unsigned char* buffer, int buffer_size, int* buffer_consumed, PNG_DECODE_HANDLE* png) {

  int consumed_size = 0;
  int bytes_per_pixel = (png->png_header.color_type == PNG_COLOR_TYPE_RGBA) ? 4 : 3;
  unsigned char* buffer_end = buffer + buffer_size;
  volatile unsigned short* gvram_current;
  
  // cropping check
  if ((png->offset_y + png->current_y) >= png->actual_height) {
    // no need to output any pixels
    *buffer_consumed = buffer_size;     // just consumed all
    return;
  }

  // GVRAM entry point
  gvram_current = (unsigned short*)GVRAM +  
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

      short r,rf;
      short g,gf;
      short b,bf;

      // before plotting, need to ensure we have accessible 3(or 4 bytes) in the inflated buffer
      // if not, we give up now and return
      if ((buffer_end - buffer) < bytes_per_pixel) {
        break;
      }

      // get raw RGB data
      r = *buffer++;
      g = *buffer++;
      b = *buffer++;

      // ignore 4th byte in RGBA mode
      if (png->png_header.color_type == PNG_COLOR_TYPE_RGBA) {
        buffer++;      
      }

      // apply filter
      switch (png->current_filter) {
      case 1:     // sub
        {
          short arf = (png->current_x > 0) ? png->left_rf : 0;
          short agf = (png->current_x > 0) ? png->left_gf : 0;
          short abf = (png->current_x > 0) ? png->left_bf : 0;
          rf = ( r + arf ) & 0xff;
          gf = ( g + agf ) & 0xff;
          bf = ( b + abf ) & 0xff;
        }
        break;
      case 2:     // up
        {
          short brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          short bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          short bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          rf = ( r + brf ) & 0xff;
          gf = ( g + bgf ) & 0xff;
          bf = ( b + bbf ) & 0xff;
        }
        break;
      case 3:     // average
        {
          short arf = (png->current_x > 0) ? png->left_rf : 0;
          short agf = (png->current_x > 0) ? png->left_gf : 0;
          short abf = (png->current_x > 0) ? png->left_bf : 0;
          short brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          short bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          short bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          rf = ( r + ((arf + brf) >> 1)) & 0xff;
          gf = ( g + ((agf + bgf) >> 1)) & 0xff;
          bf = ( b + ((abf + bbf) >> 1)) & 0xff;
        }
        break;
      case 4:     // paeth
        {
          short arf = (png->current_x > 0) ? png->left_rf : 0;
          short agf = (png->current_x > 0) ? png->left_gf : 0;
          short abf = (png->current_x > 0) ? png->left_bf : 0;
          short brf = (png->current_y > 0) ? png->up_rf_ptr[png->current_x] : 0;
          short bgf = (png->current_y > 0) ? png->up_gf_ptr[png->current_x] : 0;
          short bbf = (png->current_y > 0) ? png->up_bf_ptr[png->current_x] : 0;
          short crf = (png->current_x > 0 && png->current_y > 0) ? png->up_rf_ptr[png->current_x-1] : 0;
          short cgf = (png->current_x > 0 && png->current_y > 0) ? png->up_gf_ptr[png->current_x-1] : 0;
          short cbf = (png->current_x > 0 && png->current_y > 0) ? png->up_bf_ptr[png->current_x-1] : 0;
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
        gvram_current = (unsigned short*)GVRAM + png->actual_width * (png->offset_y + png->current_y) + png->offset_x;
      }

    }

  }

  *buffer_consumed = (buffer_size - (int)(buffer_end - buffer));
}

//
//  inflate compressed data stream
//
static int inflate_data(char* input_buffer_ptr, int input_buffer_len, int* input_buffer_consumed, 
                        char* output_buffer_ptr, int output_buffer_len,
                        z_stream* zisp, PNG_DECODE_HANDLE* png) {

  int z_status = Z_OK;
  int in_consumed_size = 0;
  
  zisp->next_in = input_buffer_ptr;
  zisp->avail_in = input_buffer_len;
  if (zisp->next_out == Z_NULL) {
    zisp->next_out = output_buffer_ptr;
    zisp->avail_out = output_buffer_len;
  }

  while (zisp->avail_in > 0) {

    int avail_in_cur = zisp->avail_in;

    // inflate
    z_status = inflate(zisp,Z_NO_FLUSH);

    if (z_status == Z_OK) {

      int in_consumed_size_this = avail_in_cur - zisp->avail_in;
      int out_inflated_size = output_buffer_len - zisp->avail_out;
      int out_consumed_size, out_remain_size;

      in_consumed_size += in_consumed_size_this;

      // output pixel
      output_pixel(output_buffer_ptr,out_inflated_size,&out_consumed_size,png);

      // in case we cannot consume all the inflated data, reuse it for the next output
      out_remain_size = out_inflated_size - out_consumed_size;
      if (out_remain_size > 0) {
        memcpy(output_buffer_ptr, output_buffer_ptr + out_consumed_size, out_remain_size);
      }

      // for next inflate operation
      zisp->next_in = input_buffer_ptr + in_consumed_size;
      zisp->next_out = output_buffer_ptr + out_remain_size;
      zisp->avail_out = output_buffer_len - out_remain_size;

    } else if (z_status == Z_STREAM_END) {

      int in_consumed_size_this = avail_in_cur - zisp->avail_in;
      int out_inflated_size = output_buffer_len - zisp->avail_out;
      int out_remain_size, out_consumed_size;

      in_consumed_size += in_consumed_size_this;

      // output pixel
      output_pixel(output_buffer_ptr,out_inflated_size,&out_consumed_size,png);

      // in case we cannot consume all the inflated data, reuse it for the next output
      out_remain_size = out_inflated_size - out_consumed_size;
      if (out_remain_size > 0) {
        memcpy(output_buffer_ptr, output_buffer_ptr + out_consumed_size, out_remain_size);
      }

      break;

    } else {
      //printf("error: data inflation error(%d).\n",z_status);
      break;
    }
  }

  *input_buffer_consumed = in_consumed_size;

  return z_status;
}

//
//  load PNG image
//
int png_load(PNG_DECODE_HANDLE* png, const char* png_file_name) {

  // return code
  int rc = -1;

  // for file operation
  FILE* fp;
  char signature[8];
  char chunk_type[5];
  int chunk_size, chunk_crc, read_size;

  // png header
  PNG_HEADER png_header;

  // for zlib inflate operation
  char* input_buffer_ptr = NULL;
  char* output_buffer_ptr = NULL;
  int input_buffer_offset = 0;
  int output_buffer_offset = 0;

  // for zlib inflate operation  
  z_stream zis;                     // inflation stream
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

  // read first 8 bytes as signature
  read_size = fread(signature, 1, 8, fp);

  // check signature
  if (png->no_signature_check == 0 && memcmp(signature,"\x89PNG\r\n\x1a\n",8) != 0 ) {
    printf("error: signature error. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }

  // allocate input buffer memory
  input_buffer_ptr = malloc_himem(png->input_buffer_size,png->use_high_memory);
  if (input_buffer_ptr == NULL) {
    printf("error: cannot allocate memory for input buffer.\n");
    goto catch;
  }
  
  // allocate output buffer memory
  output_buffer_ptr= malloc_himem(png->output_buffer_size,png->use_high_memory);
    if (output_buffer_ptr == NULL) {
    printf("error: cannot allocate memory for output buffer.\n");
    goto catch;
  }

  // process PNG file chunk by chunk
  while ((read_size = fread((char*)(&chunk_size), 1, 4, fp)) > 0) {

    // read first 4 bytes as chunk size

    // read next 4 bytes as chuck type
    fread(chunk_type, 1, 4, fp);
    chunk_type[4] = '\0';

    if (strcmp("IHDR",chunk_type) == 0) {

      // IHDR - header chunk, we can assume this chunk appears at top

#ifdef CHECK_CRC
      unsigned int checksum;
#endif

      fread(input_buffer_ptr, 1, chunk_size, fp);     // read chunk data to input buffer
      fread((char*)(&chunk_crc), 1, 4, fp);           // read 4 bytes as CRC
#ifdef CHECK_CRC
      checksum = crc32(crc32(0, chunk_type, 4), chunk_data_ptr, chunk_size);
      if (checksum != chunk_crc) {                    // check CRC if needed
        printf("error: crc error.\n");
        goto catch;
      }
#endif

      // parse header
      png_header.width              = *((int*)(input_buffer_ptr + 0));
      png_header.height             = *((int*)(input_buffer_ptr + 4));
      png_header.bit_depth          = input_buffer_ptr[8];
      png_header.color_type         = input_buffer_ptr[9];
      png_header.compression_method = input_buffer_ptr[10];
      png_header.filter_method      = input_buffer_ptr[11];
      png_header.interlace_method   = input_buffer_ptr[12];

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

    } else if (strcmp("IDAT",chunk_type) == 0) {

      // IDAT - data chunk, may appear several times
#ifdef CHECK_CRC
      unsigned int checksum;
#endif
      if (chunk_size > (png->input_buffer_size - input_buffer_offset)) {
        int input_buffer_consumed = 0;
        int z_status;
#ifdef DEBUG
        printf("no more space in input buffer, need to consume. (ofs=%d,chunksize=%d)\n",input_buffer_offset,chunk_size);
#endif
        z_status = inflate_data(input_buffer_ptr,input_buffer_offset,&input_buffer_consumed,output_buffer_ptr,png->output_buffer_size,&zis,png);
        if (z_status != Z_OK && z_status != Z_STREAM_END) {
          printf("error: zlib data decompression error(%d).\n",z_status);
          goto catch;
        }
#ifdef DEBUG
        printf("inflated. input consumed=%d\n",input_buffer_consumed);
#endif
        // in case we cannot consume all the data inflate data, move it to the top of the buffer for the following use
        if (input_buffer_consumed < input_buffer_offset) {
          memcpy(input_buffer_ptr,input_buffer_ptr+input_buffer_consumed,input_buffer_offset - input_buffer_consumed);
          input_buffer_offset = input_buffer_consumed;
        } else {
          input_buffer_offset = 0;
        }

      } else {
#ifdef DEBUG
        printf("input buffer memory is still not full. (ofs=%d,chunksize=%d)\n",input_buffer_offset,chunk_size);
#endif
      }

      // read chunk data into input buffer
      fread(input_buffer_ptr + input_buffer_offset, 1, chunk_size, fp);
      input_buffer_offset += chunk_size;

      fread((char*)(&chunk_crc), 1, 4, fp);
#ifdef CHECK_CRC
      checksum = crc32(crc32(0, chunk_type, 4), input_buffer_ptr + input_buffer_offset, chunk_size);
      if (checksum != chunk_crc) {
        printf("error: crc error.\n");
        goto catch;
      }
#endif

    } else if (strcmp("IEND",chunk_type) == 0) {

      // IEND chunk - the very last chunk
#ifdef DEBUG
      printf("found IEND.\n");
#endif
      break;

    } else {

      // unknown chunk - just skip
      fseek(fp, chunk_size + 4, SEEK_CUR);

    }

  }

  // do we have any unconsumed data?
  if (input_buffer_offset > 0) {
    int input_buffer_consumed = 0;
    int z_status = inflate_data(input_buffer_ptr,input_buffer_offset,&input_buffer_consumed,output_buffer_ptr,png->output_buffer_size,&zis,png);
    if (z_status != Z_OK && z_status != Z_STREAM_END) {
      printf("error: zlib data decompression error(%d).\n",z_status);
      goto catch;
    }
    input_buffer_offset = (input_buffer_offset == input_buffer_consumed) ? 0 : input_buffer_consumed;
  }

  // complete zlib inflation stream operation
  inflateEnd(&zis);

  // succeeded
  rc = 0;

catch:
  // close source PNG file
  fclose(fp);

  // reclaim input buffer memory
  if (input_buffer_ptr != NULL) {
    free_himem(input_buffer_ptr, png->use_high_memory);
    input_buffer_ptr = NULL;
  }

  // reclaim output buffer memory
  if (output_buffer_ptr > 0) {
    free_himem(output_buffer_ptr, png->use_high_memory);
    output_buffer_ptr = NULL;
  }

  // done
  return rc;
}

//
//  describe PNG file information
//
int png_describe(PNG_DECODE_HANDLE* png, const char* png_file_name) {

  // return code
  int rc = -1;

  // for file operation
  FILE* fp;
  char signature[8];
  char chunk_type[5];
  int chunk_size, chunk_crc, read_size;

  // png header
  PNG_HEADER png_header;

  // for zlib inflate operation
  char* input_buffer_ptr = NULL;
  int input_buffer_offset = 0;

  // open source file
  fp = fopen(png_file_name, "rb");
  if (fp == NULL) {
    printf("error: cannot open input file (%s).\n", png_file_name);
    goto catch;
  }
#ifdef DEBUG 
  printf("file opened for information\n");
#endif
  // read first 8 bytes as signature
  read_size = fread(signature, 1, 8, fp);

  // check signature
  if (png->no_signature_check == 0 && memcmp(signature,"\x89PNG\r\n\x1a\n",8) != 0 ) {
    printf("error: signature error. not a PNG file (%s).\n", png_file_name);
    goto catch;
  }

  // allocate input buffer memory
  input_buffer_ptr = malloc_himem(png->input_buffer_size,png->use_high_memory);
  if (input_buffer_ptr == NULL) {
    printf("error: cannot allocate memory for input buffer.\n");
    goto catch;
  }
  
  // process PNG file chunk by chunk
  while ((read_size = fread((char*)(&chunk_size), 1, 4, fp)) > 0) {

    // read first 4 bytes as chunk size

    // read next 4 bytes as chuck type
    fread(chunk_type, 1, 4, fp);
    chunk_type[4] = '\0';

    if (strcmp("IHDR",chunk_type) == 0) {

      // IHDR - header chunk, we can assume this chunk appears at top
      struct FILBUF inf;

#ifdef CHECK_CRC
      unsigned int checksum;
#endif

      fread(input_buffer_ptr, 1, chunk_size, fp);     // read chunk data to input buffer
      fread((char*)(&chunk_crc), 1, 4, fp);           // read 4 bytes as CRC
#ifdef CHECK_CRC
      checksum = crc32(crc32(0, chunk_type, 4), chunk_data_ptr, chunk_size);
      if (checksum != chunk_crc) {                    // check CRC if needed
        printf("error: crc error.\n");
        goto catch;
      }
#endif

      // parse header
      png_header.width              = *((int*)(input_buffer_ptr + 0));
      png_header.height             = *((int*)(input_buffer_ptr + 4));
      png_header.bit_depth          = input_buffer_ptr[8];
      png_header.color_type         = input_buffer_ptr[9];
      png_header.compression_method = input_buffer_ptr[10];
      png_header.filter_method      = input_buffer_ptr[11];
      png_header.interlace_method   = input_buffer_ptr[12];

      FILES(&inf,(unsigned char*)png_file_name,0x23);
      printf("--\n");
      printf(" file name: %s\n",png_file_name);
      printf(" file size: %d\n",inf.filelen);
      printf(" file time: %04d-%02d-%02d %02d:%02d:%02d\n",1980+(inf.date>>9),(inf.date>>5)&0xf,inf.date&0x1f,inf.time>>11,(inf.time>>5)&0x3f,inf.time&0x1f);
      printf("     width: %d\n",png_header.width);
      printf("    height: %d\n",png_header.height);
      printf(" bit depth: %d\n",png_header.bit_depth);
      printf("color type: %d\n",png_header.color_type);
      printf(" interlace: %d\n",png_header.interlace_method);
      break;

    } else {

      // skip other chunks
      fseek(fp, chunk_size + 4, SEEK_CUR);

    }

  }

  // succeeded
  rc = 0;

catch:
  // close source PNG file
  fclose(fp);

  // reclaim input buffer memory
  if (input_buffer_ptr != NULL) {
    free_himem(input_buffer_ptr, png->use_high_memory);
    input_buffer_ptr = NULL;
  }

  // done
  return rc;
}
