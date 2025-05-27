#pragma once

// Specific compatibility fix for portcls.h header
// This addresses the C2061 errors in the portcls.h file

// First include Windows headers in the correct order
#include <ntddk.h>
#include <windef.h>
#include <wdm.h>

// Ensure WAVE_FORMAT constants are defined
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

// Forward declare structures that portcls.h expects
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerBlock;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE to avoid C2061 errors
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif
