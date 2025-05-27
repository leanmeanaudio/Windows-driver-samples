#pragma once

// This header defines the necessary structures for mmreg.h in kernel-mode
// without relying on user-mode headers like wingdi.h

// Define BITMAPINFOHEADER structure that mmreg.h depends on
// This is normally defined in wingdi.h, but we're in kernel mode
#ifndef _WINGDI_
#define _WINGDI_

// Basic types needed for BITMAPINFOHEADER
typedef LONG FXPT2DOT30;
typedef LONG FXPT16DOT16;

// BITMAPINFOHEADER definition
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

// Other structures mmreg.h might need
typedef struct tagRGBQUAD {
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} RGBQUAD;

#endif // _WINGDI_

// Now include mmreg.h which will use our definitions
#include <mmreg.h>
