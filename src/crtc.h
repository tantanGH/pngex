#ifndef __H_CRTC__
#define __H_CRTC__

#ifndef GPIP
#define GPIP         ((volatile unsigned char*)0xE88001)     // generic I/O port (Inside X68000 p81)
#endif

#ifndef WAIT_VDISP
#define WAIT_VDISP     while(!(GPIP[0] & 0x10))
#define WAIT_VBLANK    while(GPIP[0] & 0x10)
#endif

// prototype declarations
void set_extra_crtc_mode(int extended_graphic_mode);

#endif