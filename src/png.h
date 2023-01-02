#ifndef __H_PNG__
#define __H_PNG__

#include <stdio.h>
#include <string.h>

// GVRAM memory address
#define GVRAM       ((volatile unsigned short*)0xC00000)

// PNG color type
#define PNG_COLOR_TYPE_RGB  2
#define PNG_COLOR_TYPE_RGBA 6

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

// PNG decode engine status handle
typedef struct {

  // input parameters
  int input_buffer_size;
  int output_buffer_size;
  int use_high_memory;
  int use_extended_graphic;
  int brightness;
  int centering;
  int offset_x;
  int offset_y;
  int no_signature_check;

  // png header copy
  PNG_HEADER png_header;

  // actual screen size (determined by extended graphic use)
  int actual_width;
  int actual_height;

  // current decode state
  int current_x;
  int current_y;
  int current_filter;

  // for filter use
  unsigned char left_rf;
  unsigned char left_gf;
  unsigned char left_bf;
  unsigned char* up_rf_ptr;
  unsigned char* up_gf_ptr;
  unsigned char* up_bf_ptr;  

  // RGB888 to RGB555 color map
  unsigned short* rgb555_r;
  unsigned short* rgb555_g;
  unsigned short* rgb555_b;

} PNG_DECODE_HANDLE;

// prototype declarations
void init_png(PNG_DECODE_HANDLE* png);
void set_png_header(PNG_DECODE_HANDLE* png, PNG_HEADER* png_header);
void quit_png(PNG_DECODE_HANDLE* png);
int load_png_image(PNG_DECODE_HANDLE* png, const char* png_file_name );
int describe_png_image(PNG_DECODE_HANDLE* png, const char* png_file_name);

#endif