#include "crtc.h"

// initialize 65536 color pallet
static void init_graphic_palette_65536() {
  int ofs = 0;
  for (int i = 0x0001; i <= 0x10000; i += 0x0202) {
    PALETTE_REG[ofs++] = (unsigned short)i;
    PALETTE_REG[ofs++] = (unsigned short)i;
  }
}

// initialize ctrc mode
void set_extra_crtc_mode(int use_extended_graphic) {

  // wait vsync
  WAIT_VDISP;
  WAIT_VBLANK;
  
  // change CRTC screen mode (768x512,31kHz) 
  // since this mode is the highest mode, we can do this operation in this order always
  CRTC_R00[0] = 0x0089;
  CRTC_R00[1] = 0x000e;
  CRTC_R00[2] = 0x001c;
  CRTC_R00[3] = 0x007c; 
  CRTC_R00[4] = 0x0237;
  CRTC_R00[5] = 0x0005;
  CRTC_R00[6] = 0x0028;
  CRTC_R00[7] = 0x0228; 
  CRTC_R00[8] = 0x001b;

  // change CRTC memory mode
  if (!use_extended_graphic) {
    CRTC_R20[0] = 0x0316;           // memory mode 3
    VDC_R0[0] = 0x0003;             // memory mode 3
    VDC_R2[0] = 0x002f;             // sprite off, text on, graphic on (512x512)
    CRTC_R12[0] = 0;                // scroll position X
    CRTC_R12[1] = 0;                // scroll position Y
    CRTC_R12[2] = 0;                // scroll position X
    CRTC_R12[3] = 0;                // scroll position Y    
    CRTC_R12[4] = 0;                // scroll position X
    CRTC_R12[5] = 0;                // scroll position Y
    CRTC_R12[6] = 0;                // scroll position X
    CRTC_R12[7] = 0;                // scroll position Y
  } else {
    CRTC_R20[0] = 0x0716;           // memory mode 7 (for XEiJ only)
    VDC_R0[0] = 0x0007;             // memory mode 7 (for XEiJ only)
    VDC_R2[0] = 0x0030;             // sprite off, text on, graphic on (1024x1024)
    CRTC_R12[0] = 0;                // scroll position X
    CRTC_R12[1] = 0;                // scroll position Y
  }

  init_graphic_palette_65536();
}
