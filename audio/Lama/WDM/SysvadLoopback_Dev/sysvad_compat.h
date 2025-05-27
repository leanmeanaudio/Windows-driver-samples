#pragma once

// Master compatibility header for Windows audio driver
// Include this ONCE at the top of your main source files

#ifndef SYSVAD_COMPAT_H
#define SYSVAD_COMPAT_H

// Disable warnings that cause issues with WDK headers
#pragma warning(disable: 4005)  // Macro redefinition
#pragma warning(disable: 4201)  // Nameless struct/union
#pragma warning(disable: 4214)  // Bit field types other than int
#pragma warning(disable: 4127)  // Conditional expression is constant

// Required includes in the correct order for Windows Driver development
#include <ntddk.h>     // Core Windows NT Driver Kit definitions
#include <wdm.h>       // Windows Driver Model definitions
#include <initguid.h>  // GUID initialization

// Include the KS headers in the correct order
#include <ks.h>        // Kernel Streaming architecture
#include <ksmedia.h>   // KS media-specific definitions

// C++ anonymous struct/union field name fix
#ifdef __cplusplus
// Define the field names as themselves to prevent them from being 
// interpreted as override specifiers in C++11 and later
#define PriorityClass PriorityClass
#define PrioritySubClass PrioritySubClass
#define Set Set
#define Id Id
#define Flags Flags
#define Alignment Alignment
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
#define SteppingDelta SteppingDelta
#define NotificationType NotificationType
#define Event Event
#define Semaphore Semaphore
#define Adjustment Adjustment
#define ObjectHandle ObjectHandle
#define MarkTime MarkTime
#define TimeBase TimeBase
#define Interval Interval
#define Manufacturer Manufacturer
#define Product Product
#define Component Component
#define Name Name
#define Version Version
#define Revision Revision
#define Current Current
#define Stop Stop
#define Earliest Earliest
#define Latest Latest
#define SourceFormat SourceFormat
#define TargetFormat TargetFormat
#define Time Time
#define FromNode FromNode
#define FromNodePin FromNodePin
#define ToNode ToNode
#define ToNodePin ToNodePin
#define CategoriesCount CategoriesCount
#define TopologyNodesCount TopologyNodesCount
#define TopologyConnectionsCount TopologyConnectionsCount
#define CreateFlags CreateFlags
#define Node Node
#define PinId PinId
#endif // __cplusplus

// Safe helper function to get channel information from KSDATARANGE structure
inline void GetSafeChannelInfo(PKSDATARANGE DataRange, ULONG* pMinChannels, ULONG* pMaxChannels) {
    // Default values
    *pMinChannels = 1;        // Default minimum
    *pMaxChannels = 2;        // Default maximum (stereo)
    
    // Pointer-based access to avoid structure layout dependencies
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
    
    // Pointer-based access to avoid structure layout dependencies
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
    
    // Pointer-based access to avoid structure layout dependencies
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

#endif // SYSVAD_COMPAT_H
