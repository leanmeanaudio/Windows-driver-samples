#pragma once

// Simple compatibility header for KSDATARANGE_AUDIO structure access
// Provides safe helper functions that work with any WDK version

#ifndef KSDATARANGE_COMPAT_H
#define KSDATARANGE_COMPAT_H

// Include Windows headers in proper order
#include <ntddk.h>     // Core Windows driver types
#include <wdm.h>       // Windows Driver Model
#include <windef.h>    // Basic Windows types
#include <ks.h>        // Kernel Streaming
#include <ksmedia.h>   // Kernel Streaming Media

// Only define these functions if they haven't been defined elsewhere
#ifndef KSDATARANGE_HELPER_FUNCTIONS_DEFINED
#define KSDATARANGE_HELPER_FUNCTIONS_DEFINED

// Safe helper function to get channel information from a KSDATARANGE structure
inline void GetSafeChannelInfo(PKSDATARANGE DataRange, ULONG* pMinChannels, ULONG* pMaxChannels) {
    // Default values
    *pMinChannels = 1;        // Default minimum
    *pMaxChannels = 2;        // Default maximum (stereo)
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + sizeof(ULONG)) {
        
        // First ULONG after KSDATAFORMAT is MaximumChannels
        PULONG pChannels = (PULONG)((BYTE*)DataRange + sizeof(KSDATAFORMAT));
        if (*pChannels > 0) {
            *pMaxChannels = *pChannels;
        }
    }
}

// Safe helper function to get bit depth information from a KSDATARANGE structure
inline void GetSafeBitDepthInfo(PKSDATARANGE DataRange, ULONG* pMinBitsPerSample, ULONG* pMaxBitsPerSample) {
    // Default values
    *pMinBitsPerSample = 16;  // Default minimum
    *pMaxBitsPerSample = 32;  // Default maximum
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG))) {
        
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

// Safe helper function to get sample rate information from a KSDATARANGE structure
inline void GetSafeSampleRateInfo(PKSDATARANGE DataRange, ULONG* pMinSampleRate, ULONG* pMaxSampleRate) {
    // Default values
    *pMinSampleRate = 8000;    // Default minimum
    *pMaxSampleRate = 192000;  // Default maximum
    
    // Pointer-based access to avoid structure layout dependencies
    if (DataRange != NULL && 
        DataRange->FormatSize >= sizeof(KSDATAFORMAT) + (5 * sizeof(ULONG))) {
        
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

#endif // KSDATARANGE_HELPER_FUNCTIONS_DEFINED

#endif // KSDATARANGE_COMPAT_H
