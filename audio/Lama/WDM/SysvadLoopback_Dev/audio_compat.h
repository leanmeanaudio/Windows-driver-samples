#pragma once

// Comprehensive fix for Windows audio driver compatibility issues
// This single header addresses structure definition conflicts in WDK headers

// Guard against multiple inclusion
#ifndef AUDIO_COMPAT_H
#define AUDIO_COMPAT_H

// First, disable warnings that can cause issues
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int

// Include standard Windows headers in the correct order
#include <ntddk.h>
#include <wdm.h>

// Define standard WAVE_FORMAT constants if not already defined
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

// Define KSAUDIO_SPEAKER constants if not already defined
#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO 0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO 0x00000003
#endif

// Forward declare structures needed for driver compilation
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

// Now it's safe to include KS headers
#include <ks.h>
#include <ksmedia.h>

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE if not already defined
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;
#endif

// Fix C++ anonymous struct/union field name issues
#ifdef __cplusplus
// Define field names that might be interpreted as C++11 override specifiers
#define Set Set
#define Id Id
#define Type Type
#define Node Node
#define PinId PinId
#define Reserved Reserved
#define Flags Flags
#endif

// Safe helper functions for accessing KSDATARANGE structures
inline void GetSafeChannelInfo(PKSDATARANGE DataRange, ULONG* pMinChannels, ULONG* pMaxChannels) {
    // Default values
    *pMinChannels = 1;        // Default minimum
    *pMaxChannels = 2;        // Default maximum (stereo)
    
    if (DataRange && DataRange->FormatSize >= sizeof(KSDATAFORMAT) + sizeof(ULONG)) {
        // First ULONG after KSDATAFORMAT is MaximumChannels
        PULONG pChannels = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT));
        if (*pChannels > 0) {
            *pMaxChannels = *pChannels;
        }
    }
}

inline void GetSafeBitDepthInfo(PKSDATARANGE DataRange, ULONG* pMinBitsPerSample, ULONG* pMaxBitsPerSample) {
    // Default values
    *pMinBitsPerSample = 16;  // Default minimum
    *pMaxBitsPerSample = 32;  // Default maximum
    
    if (DataRange && DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG))) {
        // Second ULONG after KSDATAFORMAT is MinimumBitsPerSample
        // Third ULONG after KSDATAFORMAT is MaximumBitsPerSample
        PULONG pMinBits = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + sizeof(ULONG));
        PULONG pMaxBits = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (2 * sizeof(ULONG)));
        
        if (*pMinBits > 0 && *pMaxBits > 0) {
            *pMinBitsPerSample = *pMinBits;
            *pMaxBitsPerSample = *pMaxBits;
        }
    }
}

inline void GetSafeSampleRateInfo(PKSDATARANGE DataRange, ULONG* pMinSampleRate, ULONG* pMaxSampleRate) {
    // Default values
    *pMinSampleRate = 8000;    // Default minimum
    *pMaxSampleRate = 192000;  // Default maximum
    
    if (DataRange && DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (5 * sizeof(ULONG))) {
        // Fourth ULONG after KSDATAFORMAT is MinimumSampleFrequency
        // Fifth ULONG after KSDATAFORMAT is MaximumSampleFrequency
        PULONG pMinRate = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG)));
        PULONG pMaxRate = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT) + (4 * sizeof(ULONG)));
        
        if (*pMinRate > 0 && *pMaxRate > 0) {
            *pMinSampleRate = *pMinRate;
            *pMaxSampleRate = *pMaxRate;
        }
    }
}

#endif // AUDIO_COMPAT_H
