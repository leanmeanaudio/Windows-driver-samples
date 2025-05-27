#pragma once

// Direct fix for Windows audio driver compilation issues
// This addresses the WAVEFORMATEXTENSIBLE structure definition problem

// First, we need to ensure WAVEFORMATEX is defined
#ifndef _INC_WAVEFORMATEX
#define _INC_WAVEFORMATEX

// Define WAVEFORMATEX if not already defined
typedef struct {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX;

#endif // _INC_WAVEFORMATEX

// Define WAVEFORMATEXTENSIBLE if not already defined
#ifndef _INC_WAVEFORMATEXTENSIBLE
#define _INC_WAVEFORMATEXTENSIBLE

// Define the KSAUDIO_SPEAKER constants if not already defined
#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO             0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO           0x00000003
#endif

// Define WAVEFORMATEXTENSIBLE structure
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

#endif // _INC_WAVEFORMATEXTENSIBLE

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE if not already defined
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif
