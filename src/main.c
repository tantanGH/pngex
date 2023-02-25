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
  printf("usage: pngex.x [options] <image1.png> [<image2.png> ...]\n");
  printf("options:\n");
  printf("   -c ... clear graphic screen\n");
  printf("   -n ... image centering\n");
  printf("   -k ... wait key input\n");
  printf("   -v<n> ... brightness (0-100)\n");
  printf("   -e ... use extended graphic mode (only for XEiJ)\n");
  printf("   -u ... use 060turbo/TS-6BE16 high memory for buffers\n");
  printf("   -b<n> ... buffer memory size factor[1-32] (default:8)\n");
  printf("   -z ... show only one image randomly\n");
  printf("   -i ... show file information\n");
  printf("   -h ... show this help message\n");
}

//
//  process files
//
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

//
//  main
//
int32_t main(int32_t argc, uint8_t* argv[]) {

  int32_t rc = 0;

  PNG_DECODE_HANDLE png = { 0 };

  int32_t information_mode = 0;
  int32_t clear_screen = 0;
  int32_t key_wait = 0;
  int32_t random_mode = 0;

  int32_t buffer_memory_size_factor = 4;

  int32_t input_file_count = 0;
  int32_t func_key_display_mode = 0;
 
  if (argc <= 1) {
    show_help_message();
    goto quit;
  }

  // run in supervisor mode
  B_SUPER(0);

  for (int32_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == 'e') {
        png.use_extended_graphic = 1;
      } else if (argv[i][1] == 'n') {
        png.centering = 1;
      } else if (argv[i][1] == 'u') {
        if (!himem_isavailable()) {
          printf("error: high memory driver is not installed.\n");
          rc = 1;
          goto quit;
        }
        png.use_high_memory = 1;
      } else if (argv[i][1] == 'v') {
        png.brightness = atoi(argv[i]+2);
      } else if (argv[i][1] == 'c') {
        clear_screen = 1;
      } else if (argv[i][1] == 'i') {
        information_mode = 1;
      } else if (argv[i][1] == 'k') {
        key_wait = 1;
      } else if (argv[i][1] == 'z') {
        random_mode = 1;
      } else if (argv[i][1] == 'b') {
        buffer_memory_size_factor = atoi(argv[i]+2);
        if (buffer_memory_size_factor > 32) {
          printf("error: too large memory factor.\n");
          rc = 1;
          goto quit;
        }
      } else if (argv[i][1] == 'h') {
        show_help_message();
        goto quit;
      } else {
        printf("error: unknown option (%s).\n",argv[i]);
        rc = 1;
        goto quit;
      }
    } else {
      input_file_count++;
    }
  }

  if (input_file_count <= 0) {
    printf("error: no input file.\n");
    rc = 1;
    goto quit;
  }

  // input buffer = 64KB * factor
  png.input_buffer_size = 65536 * buffer_memory_size_factor;

  // output (inflate) buffer = 128KB * factor - should be LCM(3,4)*n to store RGB or RGBA tuple
  png.output_buffer_size = 131072 * buffer_memory_size_factor;

  // init png decoder
  png_init(&png);

  if (!information_mode) {

    // check current graphic use
    int32_t usage = TGUSEMD(0,-1);
    if (usage == 1 || usage == 2) {
      printf("error: graphic vram is used by other applications.\n");
      goto catch;
    }

    // screen clear if needed
    if (clear_screen) {
      G_CLR_ON();
    }

    // initialize crtc and pallet
    set_extra_crtc_mode(png.use_extended_graphic);
    if (png.use_extended_graphic && clear_screen) {
      // manual erase
      struct FILLPTR fillptr = { 0, 0, 1023, 1023, 0 };
      FILL(&fillptr);      
    }

    // cursor display off
    C_CUROFF();

    // function key display off
    func_key_display_mode = C_FNKMOD(-1);
    C_FNKMOD(3);

  }

  // process files
  rc = process_files(argc, argv, information_mode, input_file_count, random_mode, clear_screen, key_wait, &png);

  if (!information_mode) {

    // cursor on
    C_CURON();

    // resume function key display mode
    C_FNKMOD(func_key_display_mode);
 
    // flush key buffer
    while (B_KEYSNS() != 0) {
      B_KEYINP();
    }

  }

catch:
  // close png object
  png_close(&png);

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

quit:
  return rc;
}