#pragma once

// Minimal compatibility header for Windows audio driver
// This focuses only on the essential compatibility fixes

// Disable warnings that cause issues with WDK headers
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int

// C++ anonymous struct/union field name fix for commonly used identifiers
#ifdef __cplusplus
// Define the field names as themselves to prevent them from being 
// interpreted as override specifiers in C++11 and later
#define Set Set
#define Id Id
#define Type Type
#define Reserved Reserved
#define Flags Flags
#define Node Node
#define PinId PinId
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

// Safe helper function to get bit depth information from KSDATARANGE structure
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

// Safe helper function to get sample rate information from KSDATARANGE structure
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
