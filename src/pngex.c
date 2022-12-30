#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jstring.h>
#include <time.h>
#include <doslib.h>
#include <iocslib.h>
#include "zlib.h"

#define VERSION "0.4.1"
// #define CHECK_CRC
// #define DEBUG_FWRITE
// #define DEBUG

// global variables (flags)
int g_clear_screen = 0;                 // 1:clear screen by picture
int g_extended_graphic_mode = 0;        // 1:XEiJ extended graphic mode 7
int g_information_mode = 0;             // 1:show PNG file information
int g_centering_mode = 0;               // 1:centering yes
int g_wait_mode = 0;                    // 1:wait key input
int g_reversed_scroll = 0;              // 1:cursor down = up scroll
int g_random_mode = 0;                  // 1:display one image randomly
int g_brightness = 100;                 // 0-100% brightness
int g_high_memory_mode = 0;             // 1:high memory for buffers

// global variables (buffers)
int g_buffer_memory_size_factor = 8;    // in=64KB*factor, out=128KB*factor
int g_input_buffer_size = 65536 * 8;    // default input buffer size bytes
int g_output_buffer_size = 131072 * 8;  // default output buffer size bytes

// global variables (states)
int g_actual_width = 0;                 // crop width
int g_actual_height = 0;                // crop height
int g_start_x = 0;                      // display offset X
int g_start_y = 0;                      // display offset Y
int g_current_x = -1;                   // current drawing pixel X
int g_current_y = 0;                    // current drawing pixel Y
int g_current_filter = 0;               // current PNG filter mode

//#ifdef DEBUG_FWRITE
//FILE* fp_image_data;
//#endif

// cached data for filtering
unsigned char g_left_rf = 0;
unsigned char g_left_gf = 0;
unsigned char g_left_bf = 0;
unsigned char* g_up_rf_ptr = NULL;
unsigned char* g_up_gf_ptr = NULL;
unsigned char* g_up_bf_ptr = NULL;

// RGB888 to RGB555 mapping
unsigned short g_rgb555_r[256];
unsigned short g_rgb555_g[256];
unsigned short g_rgb555_b[256];

// PNG header structure
typedef struct {
  int width;
  int height;
  char bit_depth;
  char color_type;
  char compression_method;
  char filter_method;
  char interlace_method;
} PNG_HEADER;

// PNG color type
#define PNG_COLOR_TYPE_RGB  2
#define PNG_COLOR_TYPE_RGBA 6

// graphic ops memory addresses
#define GVRAM     0xC00000
#define CRTC_R00  0xE80000
#define CRTC_R12  0xE80018
#define CRTC_R20  0xE80028
#define VDC_R1    0xE82400
#define VDC_R3    0xE82600
#define PALETTE   0xE82000
#define GPIP      0xE88001

// display screen size
#define SCREEN_WIDTH  768
#define SCREEN_HEIGHT 512

// actual screen size
#define ACTUAL_WIDTH_EX  1024
#define ACTUAL_HEIGHT_EX 1024
#define ACTUAL_WIDTH      512
#define ACTUAL_HEIGHT     512

// initialize color mapping table
static void initialize_color_mapping() {
  for (int i = 0; i < 256; i++) {
    unsigned int c = (int)(i * 32 * g_brightness / 100) >> 8;
    g_rgb555_r[i] = (unsigned short)((c <<  6) + 1);
    g_rgb555_g[i] = (unsigned short)((c << 11) + 1);
    g_rgb555_b[i] = (unsigned short)((c <<  1) + 1);
  }
}

// initialize screen
static void initialize_screen() {

  // crtc, video controller and palette
  volatile unsigned short* crtc_r00_ptr = (unsigned short*)CRTC_R00;
  volatile unsigned short* crtc_r12_ptr = (unsigned short*)CRTC_R12;
  volatile unsigned short* crtc_r20_ptr = (unsigned short*)CRTC_R20;
  volatile unsigned short* vdc_r1_ptr   = (unsigned short*)VDC_R1;
  volatile unsigned short* vdc_r3_ptr   = (unsigned short*)VDC_R3;
  volatile unsigned short* palette_ptr  = (unsigned short*)PALETTE;
  volatile unsigned char* gpip = (unsigned char*)GPIP;

  // supervisor stack pointer
  int ssp;

  // clear screen if needed
  if (g_clear_screen != 0) {
    G_CLR_ON();
  }

  // enter supervisor
  ssp = SUPER(0);

  // wait vdisp
  while(!(*gpip & 0x10));
  while( (*gpip & 0x10));

  // change CRTC screen mode (768x512,31kHz)
  crtc_r00_ptr[0] = 0x0089;
  crtc_r00_ptr[1] = 0x000e;
  crtc_r00_ptr[2] = 0x001c;
  crtc_r00_ptr[3] = 0x007c; 
  crtc_r00_ptr[4] = 0x0237;
  crtc_r00_ptr[5] = 0x0005;
  crtc_r00_ptr[6] = 0x0028;
  crtc_r00_ptr[7] = 0x0228; 
  crtc_r00_ptr[8] = 0x001b;

  // change CRTC memory mode
  if (g_extended_graphic_mode == 0) {
    *crtc_r20_ptr = 0x0316;   // memory mode 3
    *vdc_r1_ptr = 3;          // memory mode 3
    *vdc_r3_ptr = 0x2f;       // graphic on (512x512)
    crtc_r12_ptr[0] = 0;          // scroll position X
    crtc_r12_ptr[1] = 0;          // scroll position Y
    crtc_r12_ptr[2] = 0;          // scroll position X
    crtc_r12_ptr[3] = 0;          // scroll position Y    
    crtc_r12_ptr[4] = 0;          // scroll position X
    crtc_r12_ptr[5] = 0;          // scroll position Y
    crtc_r12_ptr[6] = 0;          // scroll position X
    crtc_r12_ptr[7] = 0;          // scroll position Y
  } else {
    *crtc_r20_ptr = 0x0716;   // memory mode 7 (for XEiJ only)
    *vdc_r1_ptr = 7;          // memory mode 7 (for XEiJ only)
    *vdc_r3_ptr = 0x30;       // graphic on (1024x1024)
    crtc_r12_ptr[0] = 0;          // scroll position X
    crtc_r12_ptr[1] = 0;          // scroll position Y
  }

  // initialize 65536 color pallet
  for (int i = 0x0001; i <= 0x10000; i += 0x0202) {
    *palette_ptr++ = (unsigned short)i;
    *palette_ptr++ = (unsigned short)i;
  }

  // back to user mode
  SUPER(ssp);
}

//  high memory operations
inline static void* malloc_himem(size_t size) {

    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 1;         // HIMEM_MALLOC
    in_regs.d2 = size;

    TRAP15(&in_regs, &out_regs);

    int rc = out_regs.d0;

#ifdef DEBUG
    printf("allocated high memory = %X\n",out_regs.a1);
#endif
    return (rc == 0) ? (void*)out_regs.a1 : NULL;
}

inline static void free_himem(void* addr) {
    
    struct REGS in_regs = { 0 };
    struct REGS out_regs = { 0 };

    in_regs.d0 = 0xF8;      // IOCS _HIMEM
    in_regs.d1 = 2;         // HIMEM_FREE
    in_regs.d2 = (size_t)addr;

    TRAP15(&in_regs, &out_regs);
}

//  direct memory allocation using DOSCALL (with malloc, we cannot allocate more than 64k, why?)
inline static void* malloc__(size_t size) {
  if (g_high_memory_mode) {
    return malloc_himem(size);
  }
  int addr = MALLOC(size);
  return (addr >= 0x81000000) ? NULL : (void*)addr;
}

inline static void free__(void* ptr) {
  if (ptr == NULL) return;
  if (g_high_memory_mode) {
    free_himem(ptr);
    return;
  }
  MFREE((size_t)ptr);
}

// paeth predictor for PNG filter mode 4
inline static short paeth_predictor(short a, short b, short c) {
  short p = a + b - c;
  short pa = p > a ? p - a : a - p;
  short pb = p > b ? p - b : b - p;
  short pc = p > c ? p - c : c - p;
#ifdef DEBUG
//  printf("p=%d,pa=%d,pb=%d,pc=%d,a=%d,b=%d,c=%d\n",p,pa,pb,pc,a,b,c);
#endif
  if (pa <= pb && pa <= pc) {
    return a;  
  } else if (pb <= pc) {
    return b;
  }
  return c;
}

// output pixel data
static void output_pixel(unsigned char* buffer, int buffer_size, int* buffer_consumed, PNG_HEADER* png_headerp) {

  int consumed_size = 0;
  int ssp;
  int bytes_per_pixel = (png_headerp->color_type == PNG_COLOR_TYPE_RGBA) ? 4 : 3;
  unsigned char* buffer_end = buffer + buffer_size;
  volatile unsigned short* gvram_current;
  
  // cropping check
  if ((g_start_y + g_current_y) >= g_actual_height) {
    // no need to output any pixels
    *buffer_consumed = buffer_size;     // just consumed all
    return;
  }

  // GVRAM entry point
  gvram_current = (unsigned short*)GVRAM +  
                                    g_actual_width * (g_start_y + g_current_y) + 
                                    g_start_x + ((g_current_x >= 0) ? g_current_x : 0);

  // supervisor mode
  ssp = SUPER(0);

  while (buffer < buffer_end) {

    if (g_current_x == -1) {    // first byte of each scan line

      // get filter mode
      g_current_filter = *buffer++;
#ifdef DEBUG
      //printf("g_current_filter=%d,g_current_y=%d\n",g_current_filter,g_current_y);
#endif
      // next pixel
      g_current_x++;

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
      if (png_headerp->color_type == PNG_COLOR_TYPE_RGBA) {
        buffer++;      
      }

      // apply filter
      switch (g_current_filter) {
      case 1:     // sub
        {
          short arf = (g_current_x > 0) ? g_left_rf : 0;
          short agf = (g_current_x > 0) ? g_left_gf : 0;
          short abf = (g_current_x > 0) ? g_left_bf : 0;
          rf = ( r + arf ) & 0xff;
          gf = ( g + agf ) & 0xff;
          bf = ( b + abf ) & 0xff;
        }
        break;
      case 2:     // up
        {
          short brf = (g_current_y > 0) ? g_up_rf_ptr[g_current_x] : 0;
          short bgf = (g_current_y > 0) ? g_up_gf_ptr[g_current_x] : 0;
          short bbf = (g_current_y > 0) ? g_up_bf_ptr[g_current_x] : 0;
          rf = ( r + brf ) & 0xff;
          gf = ( g + bgf ) & 0xff;
          bf = ( b + bbf ) & 0xff;
        }
        break;
      case 3:     // average
        {
          short arf = (g_current_x > 0) ? g_left_rf : 0;
          short agf = (g_current_x > 0) ? g_left_gf : 0;
          short abf = (g_current_x > 0) ? g_left_bf : 0;
          short brf = (g_current_y > 0) ? g_up_rf_ptr[g_current_x] : 0;
          short bgf = (g_current_y > 0) ? g_up_gf_ptr[g_current_x] : 0;
          short bbf = (g_current_y > 0) ? g_up_bf_ptr[g_current_x] : 0;
          rf = ( r + ((arf + brf) >> 1)) & 0xff;
          gf = ( g + ((agf + bgf) >> 1)) & 0xff;
          bf = ( b + ((abf + bbf) >> 1)) & 0xff;
        }
        break;
      case 4:     // paeth
        {
          short arf = (g_current_x > 0) ? g_left_rf : 0;
          short agf = (g_current_x > 0) ? g_left_gf : 0;
          short abf = (g_current_x > 0) ? g_left_bf : 0;
          short brf = (g_current_y > 0) ? g_up_rf_ptr[g_current_x] : 0;
          short bgf = (g_current_y > 0) ? g_up_gf_ptr[g_current_x] : 0;
          short bbf = (g_current_y > 0) ? g_up_bf_ptr[g_current_x] : 0;
          short crf = (g_current_x > 0 && g_current_y > 0) ? g_up_rf_ptr[g_current_x-1] : 0;
          short cgf = (g_current_x > 0 && g_current_y > 0) ? g_up_gf_ptr[g_current_x-1] : 0;
          short cbf = (g_current_x > 0 && g_current_y > 0) ? g_up_bf_ptr[g_current_x-1] : 0;
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
      if ((g_start_x + g_current_x) < g_actual_width) {
        *gvram_current++ = g_rgb555_r[rf] | g_rgb555_g[gf] | g_rgb555_b[bf];
      }
#ifdef DEBUG
      //printf("pixel: x=%d,y=%d,r=%d,g=%d,b=%d,rf=%d,gf=%d,bf=%d\n",g_current_x,g_current_y,r,g,b,rf,gf,bf);
#endif      
  
      // cache r,g,b for downstream filtering
      if (g_current_x > 0) {
        g_up_rf_ptr[g_current_x-1] = g_left_rf;
        g_up_gf_ptr[g_current_x-1] = g_left_gf;
        g_up_bf_ptr[g_current_x-1] = g_left_bf;
      }
      if (g_current_x == png_headerp->width-1) {
        g_up_rf_ptr[g_current_x] = rf;
        g_up_gf_ptr[g_current_x] = gf;
        g_up_bf_ptr[g_current_x] = bf;
      }
      g_left_rf = rf;
      g_left_gf = gf;
      g_left_bf = bf;

      // next pixel
      g_current_x++;

      // next scan line
      if (g_current_x >= png_headerp->width) {
        g_current_x = -1;
        g_current_y++;
        if ((g_start_y + g_current_y) >= g_actual_height) break;  // Y cropping
        gvram_current = (unsigned short*)GVRAM + g_actual_width * (g_start_y + g_current_y) + g_start_x;
      }

    }

  }

  SUPER(ssp);

  *buffer_consumed = (buffer_size - (int)(buffer_end - buffer));
}

// inflate compressed data stream
static int inflate_data(char* input_buffer_ptr, int input_buffer_len, int* input_buffer_consumed, 
                        char* output_buffer_ptr, int output_buffer_len,
                        z_stream* zisp, PNG_HEADER* png_headerp) {

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
      output_pixel(output_buffer_ptr,out_inflated_size,&out_consumed_size,png_headerp);
//#ifdef DEBUG_FWRITE
//      fwrite(output_buffer_ptr,1,out_consumed_size,fp_image_data);
//#endif

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
      output_pixel(output_buffer_ptr,out_inflated_size,&out_consumed_size,png_headerp);
//#ifdef DEBUG_FWRITE
//      fwrite(output_buffer_ptr,1,out_consumed_size,fp_image_data);
//#endif

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

// decode PNG
static int decode_png_image(char* filename) {

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
    return -1;
  }
  
//  #ifdef DEBUG_FWRITE
//  fp_image_data = fopen("__debug_.raw", "wb");
//  #endif

  // open source file
  fp = fopen(filename, "rb");
  if (fp == NULL) {
    printf("error: cannot open input file (%s).\n", filename);
    return -1;
  }

  // read first 8 bytes as signature
  read_size = fread(signature, 1, 8, fp);

  // check signature
  if (memcmp(signature,"\x89PNG\r\n\x1a\n",8) != 0 ) {
    printf("error: signature error. not a PNG file (%s).\n", filename);
    fclose(fp);
    return -1;
  }

  // allocate input buffer memory
  input_buffer_ptr = malloc__(g_input_buffer_size);
  if (input_buffer_ptr == NULL) {
    printf("error: cannot allocate memory for input buffer.\n");
    fclose(fp);
    return -1;
  }
  
  // allocate output buffer memory
  output_buffer_ptr= malloc__(g_output_buffer_size);
    if (output_buffer_ptr == NULL) {
    printf("error: cannot allocate memory for output buffer.\n");
    fclose(fp);
    free__(input_buffer_ptr);
    return -1;
  }
  
  // process PNG file chunk by chunk
  while ((read_size = fread((char*)(&chunk_size), 1, 4, fp)) > 0) {

    // read first 4 bytes as chunk size

    // read next 4 bytes as chuck type
    fread(chunk_type, 1, 4, fp);
    chunk_type[4] = '\0';

#ifdef DEBUG
    printf("chunk_type=%s\n",chunk_type);
    printf("chunk_size=%d\n",chunk_size);
#endif

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
        printf("warning: crc error.\n");
      }
#endif

      // parse header
      png_header.width              = *((int*)(input_buffer_ptr));
      png_header.height             = *((int*)(input_buffer_ptr + 4));
      png_header.bit_depth          = input_buffer_ptr[8];
      png_header.color_type         = input_buffer_ptr[9];
      png_header.compression_method = input_buffer_ptr[10];
      png_header.filter_method      = input_buffer_ptr[11];
      png_header.interlace_method   = input_buffer_ptr[12];
      //memcpy((char*)&png_header,input_buffer_ptr,sizeof(PNG_HEADER));   // this might not work properly because of gcc's alignment optimization
      if (g_information_mode != 0) {
        struct FILBUF inf;
        FILES(&inf,filename,0x23);
        printf("--\n");
        printf(" file name: %s\n",filename);
        printf(" file size: %d\n",inf.filelen);
        printf(" file time: %04d-%02d-%02d %02d:%02d:%02d\n",1980+(inf.date>>9),(inf.date>>5)&0xf,inf.date&0x1f,inf.time>>11,(inf.time>>5)&0x3f,inf.time&0x1f);
        printf("     width: %d\n",png_header.width);
        printf("    height: %d\n",png_header.height);
        printf(" bit depth: %d\n",png_header.bit_depth);
        printf("color type: %d\n",png_header.color_type);
        printf(" interlace: %d\n",png_header.interlace_method);
        break;
      }

      // check bit depth (support 8bit color only)
      if (png_header.bit_depth != 8) {
        printf("error: unsupported bit depth (%d).\n",png_header.bit_depth);
        break;
      }

      // check color type (support RGB or RGBA only)
      if (png_header.color_type != 2 && png_header.color_type != 6) {
        printf("error: unsupported color type (%d).\n",png_header.color_type);
        break;
      }

      // check interlace mode
      if (png_header.interlace_method != 0) {
        printf("error: interlace png is not supported.\n");
        break;
      }

      // allocate buffer memory for upper scanline filtering
      g_up_rf_ptr = malloc__(png_header.width);
      g_up_gf_ptr = malloc__(png_header.width);
      g_up_bf_ptr = malloc__(png_header.width);

      // start offset calculation
      if (g_centering_mode != 0) {
        g_start_x = (png_header.width <= SCREEN_WIDTH) ? (SCREEN_WIDTH - png_header.width) >> 1 : 0;
        g_start_y = (png_header.height <= SCREEN_HEIGHT) ? (SCREEN_HEIGHT - png_header.height) >> 1 : 0;
      } else {
        g_start_x = 0;
        g_start_y = 0;
      }

      // initialize pixel positions
      g_current_x = -1;
      g_current_y = 0;
      g_current_filter = 0;

    } else if (strcmp("IDAT",chunk_type) == 0) {

      // IDAT - data chunk, may appear several times

#ifdef CHECK_CRC
      unsigned int checksum;
#endif
      if (chunk_size > (g_input_buffer_size - input_buffer_offset)) {
        int input_buffer_consumed = 0;
        int z_status;
#ifdef DEBUG
        printf("no more space in input buffer, need to consume. (ofs=%d,chunksize=%d)\n",input_buffer_offset,chunk_size);
#endif
        z_status = inflate_data(input_buffer_ptr,input_buffer_offset,&input_buffer_consumed,output_buffer_ptr,g_output_buffer_size,&zis,&png_header);
        if (z_status != Z_OK && z_status != Z_STREAM_END) {
          printf("error: zlib data decompression error(%d).\n",z_status);
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
        printf("warning: crc error.\n");
      }
#endif

    } else if (strcmp("IEND",chunk_type) == 0) {

      // IEND chunk - the very last chunk

#ifdef DEBUG
      printf("found IEND.\n");
#endif
      break;

    } else {
      fseek(fp, chunk_size + 4, SEEK_CUR);
    }

  }

  // do we have any unconsumed data?
  if (input_buffer_offset > 0) {
    int input_buffer_consumed = 0;
    int z_status = inflate_data(input_buffer_ptr,input_buffer_offset,&input_buffer_consumed,output_buffer_ptr,g_output_buffer_size,&zis,&png_header);
    if (z_status != Z_OK && z_status != Z_STREAM_END) {
      printf("error: zlib data decompression error(%d).\n",z_status);
    }
    input_buffer_offset = (input_buffer_offset == input_buffer_consumed) ? 0 : input_buffer_consumed;
  }

  // complete zlib inflation stream operation
  inflateEnd(&zis);

  // close source PNG file
  fclose(fp);

//#ifdef DEBUG_FWRITE
//  fclose(fp_image_data);
//#endif

  // reclaim filter buffer memory
  if (g_up_rf_ptr != NULL) {
    free__(g_up_rf_ptr);
    g_up_rf_ptr = NULL;
  }
  if (g_up_gf_ptr != NULL) {
    free__(g_up_gf_ptr);
    g_up_gf_ptr = NULL;
  }
  if (g_up_bf_ptr != NULL) {
    free__(g_up_bf_ptr);
    g_up_bf_ptr = NULL;
  }

  // reclaim input buffer memory
  if (input_buffer_ptr != NULL) {
    free__(input_buffer_ptr);
    input_buffer_ptr = NULL;
  }

  // reclaim output buffer memory
  if (output_buffer_ptr > 0) {
    free__(output_buffer_ptr);
    output_buffer_ptr = NULL;
  }

  // key wait
  if (g_wait_mode != 0) {
    getchar();
  }

  // done
  return 0;
}

// show help messages
static void show_help_message() {
  printf("PNGEX - PNG image loader with XEiJ graphic extension support version " VERSION " by tantan 2022\n");
  printf("usage: pngex.x [options] <image1.png> [<image2.png> ...]\n");
  printf("options:\n");
  printf("   -c ... clear graphic screen\n");
  printf("   -e ... use extended graphic mode for XEiJ (1024x1024x65536)\n");
  printf("   -h ... show this help message\n");
  printf("   -i ... show file information\n");
  printf("   -n ... image centering\n");
  printf("   -k ... wait key input\n");
  printf("   -u ... use high memory for buffers (set -b32 automatically)\n");
  printf("   -v<n> ... brightness (0-100)\n");
  printf("   -z ... show only one image randomly\n");
  printf("   -b<n> ... buffer memory size factor[1-32] (default:8)\n");
}

// main
int main(int argc, char* argv[]) {

  int input_file_count = 0;
  int func_key_display_mode = 0;
 
  if (argc <= 1) {
    show_help_message();
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == 'c') {
        g_clear_screen = 1;
      } else if (argv[i][1] == 'e') {
        g_extended_graphic_mode = 1;
      } else if (argv[i][1] == 'i') {
        g_information_mode = 1;
      } else if (argv[i][1] == 'n') {
        g_centering_mode = 1;
      } else if (argv[i][1] == 'k') {
        g_wait_mode = 1;
      } else if (argv[i][1] == 'u') {
        g_high_memory_mode = 1;
        g_buffer_memory_size_factor = 32;
      } else if (argv[i][1] == 'z') {
        g_random_mode = 1;
      } else if (argv[i][1] == 'v') {
        g_brightness = atoi(argv[i]+2);
      } else if (argv[i][1] == 'b') {
        g_buffer_memory_size_factor = atoi(argv[i]+2);
        if (g_buffer_memory_size_factor > 32) {
          printf("error: too large memory factor.\n");
          return 1;
        }
      } else if (argv[i][1] == 'h') {
        show_help_message();
        return 0;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        return 1;
      }
    } else {
      input_file_count++;
    }
  }

  if (input_file_count <= 0) {
    printf("error: no input file.\n");
    return 1;
  }

  if (g_information_mode != 1) {

    // input buffer = 64KB * factor
    g_input_buffer_size = 65536 * g_buffer_memory_size_factor;

    // output (inflate) buffer = 128KB * factor - should be LCM(3,4)*n to store RGB or RGBA tuple
    g_output_buffer_size = 131072 * g_buffer_memory_size_factor;

    // cropping window
    g_actual_width = g_extended_graphic_mode != 0 ? ACTUAL_WIDTH_EX : ACTUAL_WIDTH;
    g_actual_height = g_extended_graphic_mode != 0 ? ACTUAL_HEIGHT_EX : ACTUAL_HEIGHT;

    // initialize color mapping table
    initialize_color_mapping();

    // initialize graphic screen
    initialize_screen();

    // cursor display off
    C_CUROFF();

    // function key display off
    func_key_display_mode = C_FNKMOD(-1);
    C_FNKMOD(3);

  }

  // decode png image
  {
    int file_index = 0;
    int random_index = 0;
    if (g_random_mode != 0) {
      srand((unsigned int)time(NULL));
      random_index = rand() % input_file_count;
#ifdef DEBUG
      printf("random_index=%d\n",random_index);
#endif
    }
    for (int i = 1; i < argc; i++) {
      char* file_name = argv[i];
      if (file_name[0] == '-') continue;
      if (g_random_mode == 0 || file_index == random_index) {
        if (jstrchr(file_name,'*') == NULL && jstrchr(file_name,'?') == NULL) {
          // single file
          decode_png_image(file_name);
        } else {
          // expand wild card
          struct FILBUF inf;
          char path_name[256];
          char* c;
          int rc, wild_file_index = 0, wild_random_index = 0;
          strcpy(path_name,file_name);
          if ((c = jstrrchr(path_name,'\\')) != NULL ||
              (c = jstrrchr(path_name,'/')) != NULL ||
              (c = jstrrchr(path_name,':')) != NULL) {
            *(c+1) = '\0';
          } else {
            path_name[0] = '\0';
          }
          rc = FILES(&inf,file_name,0x20);
          if (rc != 0) {
            printf("error: file search error. (rc=%d)\n",rc);
            break;
          }
          if (g_random_mode != 0) {
            int wild_file_count = 0;
            while (rc == 0) {
              wild_file_count++;
              rc = NFILES(&inf);
            }
            srand((unsigned int)time(NULL));
            wild_random_index = rand() % wild_file_count;
            rc = FILES(&inf,file_name,0x20);
          }
          while (rc == 0) {
            if (g_random_mode == 0 || wild_file_index == wild_random_index) {
              char this_name[256];
              strcpy(this_name,path_name);
              strcat(this_name,inf.name);
              decode_png_image(this_name);
            }
            wild_file_index++;
            rc = NFILES(&inf);
          }
        }
      }
      file_index++;
    }
  }

  if (g_information_mode != 1) {

    // cursor on
    C_CURON();

    // resume function key display mode
    C_FNKMOD(func_key_display_mode);

  }

  return 0;
}