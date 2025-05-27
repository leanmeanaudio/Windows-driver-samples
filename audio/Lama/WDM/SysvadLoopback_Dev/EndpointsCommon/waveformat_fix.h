#pragma once

// Special fix for WAVEFORMATEXTENSIBLE and KSDATAFORMAT_WAVEFORMATEXTENSIBLE structure compatibility
// This only defines what we need without conflicting with WDK definitions

// Disable specific warnings that can cause issues with our definitions
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4214) // bit field types other than int
#pragma warning(disable: 4115) // named type definition in parentheses

// Only define these if not already defined elsewhere
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef KSAUDIO_SPEAKER_MONO
#define KSAUDIO_SPEAKER_MONO 0x00000004
#endif

#ifndef KSAUDIO_SPEAKER_STEREO
#define KSAUDIO_SPEAKER_STEREO 0x00000003
#endif

// Define KSDATAFORMAT_WAVEFORMATEXTENSIBLE without redefining existing structures
#ifndef KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED
#define KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED

// If the Windows headers haven't defined WAVEFORMATEX, provide our own minimal definition
#ifndef MMSYSERR_NOERROR
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

// If the Windows headers haven't defined WAVEFORMATEXTENSIBLE, provide our own
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

// Define our own KSDATAFORMAT_WAVEFORMATEXTENSIBLE regardless
typedef struct {
    KSDATAFORMAT DataFormat;
    WAVEFORMATEXTENSIBLE WaveFormatExt;
} KSDATAFORMAT_WAVEFORMATEXTENSIBLE, *PKSDATAFORMAT_WAVEFORMATEXTENSIBLE;

#endif // KSDATAFORMAT_WAVEFORMATEXTENSIBLE_DEFINED

// Define STATICGUIDOF macro if not already defined
#ifndef STATICGUIDOF
#define STATICGUIDOF(guid) STATIC_##guid
#endif

// Safe helper functions for KSDATARANGE structure access
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
