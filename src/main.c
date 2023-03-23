#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <jstring.h>
#include <time.h>
#include <doslib.h>
#include <iocslib.h>
#include <zlib.h>
#include "crtc.h"
#include "himem.h"
#include "png.h"
#include "pngex.h"

//
//  show help messages
//
static void show_help_message() {
  printf("PNGEX - PNG image loader for X680x0 version " VERSION " by tantan\n");
  printf("usage: pngex.x [options] <image.png>\n");
  printf("options:\n");
  printf("   -v<n> ... brightness (0-100)\n");
  printf("   -c ... clear graphic screen\n");
//  printf("   -n ... image centering\n");
//  printf("   -k ... wait key input\n");
  printf("   -e ... use XEiJ extended graphic mode\n");
//  printf("   -u ... use 060turbo/TS-6BE16 high memory for buffers\n");
//  printf("   -b<n> ... buffer memory size factor[1-32] (default:8)\n");
//  printf("   -z ... show only one image randomly\n");
//  printf("   -i ... show file information\n");
  printf("   -h ... show this help message\n");
}

//
//  process files
//
/*
static int32_t process_files(int32_t argc, uint8_t* argv[], int32_t information_mode, int32_t input_file_count, int32_t random_mode, int32_t clear_screen, int32_t key_wait, PNG_DECODE_HANDLE* png) {

  int32_t file_index = 0;
  int32_t random_index = 0;

  if (random_mode) {
    srand((uint32_t)time(NULL));
    random_index = rand() % input_file_count;
  }

  for (int32_t i = 1; i < argc; i++) {

    uint8_t* file_name = argv[i];

    if (file_name[0] == '-') continue;

    if (!random_mode || file_index == random_index) {

      if (jstrchr(file_name,'*') == NULL && jstrchr(file_name,'?') == NULL) {

        // single file
        if (information_mode) {
          png_describe(png, file_name);
        } else {
          png_load(png, file_name);
        }
        if (key_wait) {
          while (B_KEYSNS() == 0) {
            ;
          }
          while (B_KEYSNS() != 0) {
            B_KEYINP();
          }
        }

      } else {

        // expand wild card
        struct FILBUF inf;
        uint8_t path_name[256];
        uint8_t* c;
        int32_t rc, wild_file_index = 0, wild_random_index = 0;

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

        if (random_mode) {
          int32_t wild_file_count = 0;
          while (rc == 0) {
            wild_file_count++;
            rc = NFILES(&inf);
          }
          srand((uint32_t)time(NULL));
          wild_random_index = rand() % wild_file_count;
          rc = FILES(&inf,file_name,0x20);
        }

        while (rc == 0) {
          if (!random_mode || wild_file_index == wild_random_index) {
            static uint8_t this_name[256];
            strcpy(this_name,path_name);
            strcat(this_name,inf.name);

            if (information_mode) {
              png_describe(png, this_name);
            } else {
              png_load(png, this_name);
            }
            if (key_wait) {
              while (B_KEYSNS() == 0) {
                ;
              }
              while (B_KEYSNS() != 0) {
                B_KEYINP();
              }
            }
          }
          wild_file_index++;
          rc = NFILES(&inf);
        }
      }
    }
    file_index++;
  }
}
*/

//
//  main
//
int32_t main(int32_t argc, uint8_t* argv[]) {

  int32_t rc = 1;

  int16_t brightness = 100;
  int16_t clear_screen = 0;
  int16_t extended_graphic = 0;
  int16_t buffer_size = 4;
  int16_t input_file_count = 0;
  int16_t func_key_display_mode = 0;

  uint8_t* png_file_name = NULL;

  PNG_DECODE_HANDLE png = { 0 };

//  int16_t information_mode = 0;
//  int16_t key_wait = 0;
//  int16_t random_mode = 0; 

  if (argc <= 1) {
    show_help_message();
    goto exit;
  }

  for (int32_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == 'e') {
        extended_graphic = 1;
//      } else if (argv[i][1] == 'n') {
//        png.centering = 1;
//      } else if (argv[i][1] == 'u') {
//        if (!himem_isavailable()) {
//          printf("error: high memory driver is not installed.\n");
//          rc = 1;
//          goto quit;
//        }
//        png.use_high_memory = 1;
      } else if (argv[i][1] == 'v') {
        brightness = atoi(argv[i]+2);
        if (brightness < 1 || brightness > 100) {
          show_help_message();
          goto exit;
        }
      } else if (argv[i][1] == 'c') {
        clear_screen = 1;
//      } else if (argv[i][1] == 'i') {
//        information_mode = 1;
//      } else if (argv[i][1] == 'k') {
//        key_wait = 1;
//      } else if (argv[i][1] == 'z') {
//        random_mode = 1;
//      } else if (argv[i][1] == 'b') {
//        buffer_memory_size_factor = atoi(argv[i]+2);
//        if (buffer_memory_size_factor > 32) {
//          printf("error: too large memory factor.\n");
//          rc = 1;
//          goto quit;
//        }
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto exit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        goto exit;
      }
    } else {
      if (png_file_name != NULL) {
        printf("error: too many png files.\n");
        goto exit;
      }
      png_file_name = argv[i];
    }
  }

  if (png_file_name == NULL) {
    printf("error: no input file.\n");
    goto exit;
  }

  // input buffer = 64KB * factor
//  png.input_buffer_size = 65536 * buffer_memory_size_factor;

  // output (inflate) buffer = 128KB * factor - should be LCM(3,4)*n to store RGB or RGBA tuple
//  png.output_buffer_size = 131072 * buffer_memory_size_factor;

  // init png decoder
  png_init(&png, buffer_size, brightness, extended_graphic);

//  if (!information_mode) {

    // check current graphic use
//    int32_t usage = TGUSEMD(0,-1);
//    if (usage == 1 || usage == 2) {
//      printf("error: graphic vram is used by other applications.\n");
//      goto catch;
//    }

  // screen clear if needed
  if (clear_screen) {
    G_CLR_ON();
    C_CLS_AL();
  }

  // run in supervisor mode
  B_SUPER(0);

  // initialize crtc and pallet
  set_extra_crtc_mode(extended_graphic);
  if (extended_graphic && clear_screen) {
    // manual erase
    struct FILLPTR fillptr = { 0, 0, 767, 511, 0 };
    FILL(&fillptr);      
  }

  // cursor display off
  C_CUROFF();

  // function key display off
  func_key_display_mode = C_FNKMOD(-1);
  C_FNKMOD(3);

//  }

  // process files
//  rc = process_files(argc, argv, information_mode, input_file_count, random_mode, clear_screen, key_wait, &png);
  png_load(&png, png_file_name);

//  if (!information_mode) {

  // cursor on
  C_CURON();

  // resume function key display mode
  C_FNKMOD(func_key_display_mode);
 
  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

//  }

catch:
  // close png object
  png_close(&png);

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

exit:
  return rc;
}