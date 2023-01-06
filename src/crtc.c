#include "crtc.h"

// graphic ops memory addresses
#define CRTC_R00    ((volatile unsigned short*)0xE80000)     // CRTC R00-R08 (Inside X68000 p232)
#define CRTC_R12    ((volatile unsigned short*)0xE80018)     // CRTC R12 for scroll (Insite X68000 p197)
#define CRTC_R20    ((volatile unsigned short*)0xE80028)     // CRTC R20 (Inside X68000 p234)
#define VDC_R0      ((volatile unsigned short*)0xE82400)     // video controller (Inside X68000 p234) *R1 = p188
#define VDC_R2      ((volatile unsigned short*)0xE82600)     // video controller (Inside X68000 p210)
#define PALETTE_REG ((volatile unsigned short*)0xE82000)     // graphic palette (Inside X68000 p218)

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
