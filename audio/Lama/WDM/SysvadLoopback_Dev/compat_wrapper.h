#pragma once

// Essential compatibility wrapper for Windows Audio Driver compilation
// Include this at the top of your main driver files instead of directly including Windows headers

#ifndef COMPAT_WRAPPER_H
#define COMPAT_WRAPPER_H

// Disable warnings that cause issues with WDK headers
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4100)  // Unreferenced formal parameter
#pragma warning(disable: 4127)  // Conditional expression is constant
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int
#pragma warning(disable: 4996)  // Deprecated functions

// Special handling for C++ override specifier issues in anonymous structs/unions
#ifdef __cplusplus
    // These definitions ensure field names in anonymous structs/unions aren't 
    // interpreted as C++ override specifiers
    #define Set Set
    #define Id Id
    #define Flags Flags
    #define NodeId NodeId
    #define Reserved Reserved
    #define Size Size
    #define Count Count
    #define AccessFlags AccessFlags
    #define DescriptionSize DescriptionSize
    #define MembersListCount MembersListCount
    #define MembersFlags MembersFlags
    #define MembersSize MembersSize
    #define MembersCount MembersCount
    #define SignedMinimum SignedMinimum
    #define SignedMaximum SignedMaximum
    #define UnsignedMinimum UnsignedMinimum
    #define UnsignedMaximum UnsignedMaximum
    #define Granularity Granularity
    #define Relation Relation
    #define Type Type
    #define MembersHeader MembersHeader
    #define x x
    #define y y
    #define z z
    #define dvX dvX
    #define dvY dvY
    #define dvZ dvZ
    #define DistanceFactor DistanceFactor
    #define RolloffFactor RolloffFactor
    #define DopplerFactor DopplerFactor
    #define MinDistance MinDistance
    #define PriorityClass PriorityClass
    #define PrioritySubClass PrioritySubClass
    #define Alignment Alignment
#endif

// Standard Windows headers with compatibility applied
#include <wdm.h>
#include <ks.h>
#include <ksmedia.h>

// Special handling for KSDATARANGE_AUDIO structure access
// These functions work with any WDK version

// Safe access to channel info in KSDATARANGE_AUDIO
inline void GetSafeKsChannelInfo(PKSDATAFORMAT DataFormat, PULONG pMinChannels, PULONG pMaxChannels) {
    // Default safe values
    *pMinChannels = 1;
    *pMaxChannels = 2;
    
    // Check if structure is large enough and has expected format
    if (DataFormat && 
        DataFormat->FormatSize >= sizeof(KSDATAFORMAT) + sizeof(ULONG) &&
        IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) &&
        IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        
        // Access the MaximumChannels field which is the first ULONG after KSDATAFORMAT
        PULONG pChannels = (PULONG)((PBYTE)DataFormat + sizeof(KSDATAFORMAT));
        if (*pChannels > 0) {
            *pMaxChannels = *pChannels;
        }
    }
}

// Safe access to bit depth info in KSDATARANGE_AUDIO
inline void GetSafeKsBitDepthInfo(PKSDATAFORMAT DataFormat, PULONG pMinBits, PULONG pMaxBits) {
    // Default safe values
    *pMinBits = 16;
    *pMaxBits = 32;
    
    // Check if structure is large enough and has expected format
    if (DataFormat && 
        DataFormat->FormatSize >= sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG)) &&
        IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) &&
        IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        
        // Access the bit depth fields (MinimumBitsPerSample and MaximumBitsPerSample)
        PULONG pMinBitsField = (PULONG)((PBYTE)DataFormat + sizeof(KSDATAFORMAT) + sizeof(ULONG));
        PULONG pMaxBitsField = (PULONG)((PBYTE)DataFormat + sizeof(KSDATAFORMAT) + (2 * sizeof(ULONG)));
        
        if (*pMinBitsField > 0 && *pMaxBitsField > 0) {
            *pMinBits = *pMinBitsField;
            *pMaxBits = *pMaxBitsField;
        }
    }
}

// Safe access to sample rate info in KSDATARANGE_AUDIO
inline void GetSafeKsSampleRateInfo(PKSDATAFORMAT DataFormat, PULONG pMinRate, PULONG pMaxRate) {
    // Default safe values
    *pMinRate = 8000;
    *pMaxRate = 192000;
    
    // Check if structure is large enough and has expected format
    if (DataFormat && 
        DataFormat->FormatSize >= sizeof(KSDATAFORMAT) + (5 * sizeof(ULONG)) &&
        IsEqualGUIDAligned(DataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) &&
        IsEqualGUIDAligned(DataFormat->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
        
        // Access the sample rate fields (MinimumSampleFrequency and MaximumSampleFrequency)
        PULONG pMinRateField = (PULONG)((PBYTE)DataFormat + sizeof(KSDATAFORMAT) + (3 * sizeof(ULONG)));
        PULONG pMaxRateField = (PULONG)((PBYTE)DataFormat + sizeof(KSDATAFORMAT) + (4 * sizeof(ULONG)));
        
        if (*pMinRateField > 0 && *pMaxRateField > 0) {
            *pMinRate = *pMinRateField;
            *pMaxRate = *pMaxRateField;
        }
    }
}

#endif // COMPAT_WRAPPER_H
