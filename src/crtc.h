#ifndef __H_CRTC__
#define __H_CRTC__

// graphic ops memory addresses
#define CRTC_R00    ((volatile unsigned short*)0xE80000)     // CRTC R00-R08 (Inside X68000 p232)
#define CRTC_R12    ((volatile unsigned short*)0xE80018)     // CRTC R12 for scroll (Insite X68000 p197)
#define CRTC_R20    ((volatile unsigned short*)0xE80028)     // CRTC R20 (Inside X68000 p234)
#define VDC_R0      ((volatile unsigned short*)0xE82400)     // video controller (Inside X68000 p234) *R1 = p188
#define VDC_R2      ((volatile unsigned short*)0xE82600)     // video controller (Inside X68000 p210)
#define PALETTE_REG ((volatile unsigned short*)0xE82000)     // graphic palette (Inside X68000 p218)
#define GPIP         ((volatile unsigned char*)0xE88001)     // generic I/O port (Inside X68000 p81)

// wait vsync/vblank
#define WAIT_VDISP     while(!(GPIP[0] & 0x10))
#define WAIT_VBLANK    while(GPIP[0] & 0x10)

// prototype declarations
void init_crtc(int extended_graphic_mode);
void init_palette(void);

#endif