#pragma once

// Master wrapper to fix C++ compatibility issues with Windows audio structures
// This header should be included before any system headers

// Disable warnings about bit-fields and anonymous structures
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // bit field types other than int
#pragma warning(disable: 4505) // unreferenced local function has been removed
#pragma warning(disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 4127) // conditional expression is constant

// Ensure C++ doesn't interfere with bit-field definitions in structures
#ifndef __cplusplus
#define FIELD_MASK(from, to)      (((1 << ((to) - (from) + 1)) - 1) << (from))
#endif

// Define Windows types that might be missing
#ifndef WINAPI
#define WINAPI __stdcall
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Ensure standard audio constants are defined to avoid redefinition errors
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO 0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO 0x00000003
#endif

// Force C mode for specific headers to avoid C++ bit-field issues
#ifdef __cplusplus
extern "C" {
#endif

// Define standard WAVEFORMATEX structure with tagged name to avoid conflicts
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct tWAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX;
#endif

// Define standard WAVEFORMATEXTENSIBLE structure with tagged name
#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct tWAVEFORMATEXTENSIBLE {
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

// Define standard KSDATAFORMAT_WAVEFORMATEXTENSIBLE structure with tagged name
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct tKSDATAFORMAT_WAVEFORMATEXTENSIBLE {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Fix for bit-field issues in KS structures
#define MAKE_KSSTRUCTURE_BITFIELD_WORK

#ifdef MAKE_KSSTRUCTURE_BITFIELD_WORK
// Define structure field helpers
#define DEFINE_BOOL_FIELD(name) ULONG name
#define DEFINE_BIT_FIELD(name, bits) ULONG name

// Redefine problematic bit-field structs as regular fields
typedef struct _KS_FIXED_LOOPED_STRUCT {
    DEFINE_BOOL_FIELD(Looped);
    DEFINE_BOOL_FIELD(InROM);
} KS_FIXED_LOOPED_STRUCT, *PKS_FIXED_LOOPED_STRUCT;

typedef struct _KS_FIXED_ERROR_STRUCT {
    DEFINE_BOOL_FIELD(fRepeatPreviousBlock);
    DEFINE_BOOL_FIELD(fErrorInCurrentBlock);
} KS_FIXED_ERROR_STRUCT, *PKS_FIXED_ERROR_STRUCT;

typedef struct _KS_FIXED_STEREO_STRUCT {
    DEFINE_BOOL_FIELD(fStereo);
} KS_FIXED_STEREO_STRUCT, *PKS_FIXED_STEREO_STRUCT;

typedef struct _KS_FIXED_DOWNMIX_STRUCT {
    DEFINE_BOOL_FIELD(fDownMix);
    DEFINE_BOOL_FIELD(fDolbySurround);
} KS_FIXED_DOWNMIX_STRUCT, *PKS_FIXED_DOWNMIX_STRUCT;

typedef struct _KS_FIXED_ROOM_STRUCT {
    DEFINE_BOOL_FIELD(fLargeRoom);
} KS_FIXED_ROOM_STRUCT, *PKS_FIXED_ROOM_STRUCT;

typedef struct _KS_FIXED_CHANNEL_STRUCT {
    DEFINE_BIT_FIELD(PrimaryChannelCount, 8);
    DEFINE_BIT_FIELD(PrimaryChannelStartPosition, 8);
    DEFINE_BIT_FIELD(PrimaryChannelMask, 32);
    DEFINE_BIT_FIELD(InterleavedChannelCount, 8);
    DEFINE_BIT_FIELD(InterleavedChannelStartPosition, 8);
    DEFINE_BIT_FIELD(InterleavedChannelMask, 32);
} KS_FIXED_CHANNEL_STRUCT, *PKS_FIXED_CHANNEL_STRUCT;
#endif

// Common audio constants
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO 0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO 0x00000003
#endif

#ifdef __cplusplus
}
#endif

// Include the KSDATARANGE helper functions from the common header
#include "ksdatarange_compat.h"
