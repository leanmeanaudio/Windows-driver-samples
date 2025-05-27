#pragma once

// Include necessary headers
#include "lamaloopbackcommon.h"  // For GUID declarations
#include "minipairs.h"          // For miniport pairs
#include "EndpointsCommon\mintopo.h"   // Base topology class
#include "EndpointsCommon\minwavert.h" // Base wave class
#include "lamaloopbackrender.h" // This already contains all the needed descriptors

// DO NOT declare or define classes or data structures that are already defined in lamaloopbackrender.h
// Instead, include that file and use what's already defined there

// We don't need to declare any external variables since they're already defined in lamaloopbackrender.h
// and we've included that file at the top

// Do not define the CMiniportTopologyLamaLoopbackCapture or CMiniportWaveRTLamaLoopbackCapture classes here
// since they are already defined in lamaloopbackrender.h

#ifndef MICINTOPO_DEFINES_ONLY
#define MICINTOPO_DEFINES_ONLY

// Define any constants or macros specific to the mic topology that aren't in other headers
#define MIC_TOPO_MAX_CHANNELS 2

// Function prototypes for functions defined in this file's corresponding .cpp file
NTSTATUS
InitMicInMinipair(_In_ PENDPOINT_MINIPAIR pMicInMinipair);

#endif // MICINTOPO_DEFINES_ONLY

// Forward class declarations
class CMiniportTopologyLamaLoopbackCapture;
class CMiniportWaveRTLamaLoopbackCapture;
typedef CMiniportWaveRTLamaLoopbackCapture *PCMiniportWaveRTLamaLoopbackCapture;

// Function declarations for factory methods
NTSTATUS CreateMiniportTopologyLamaLoopbackCapture(
    _Out_ PUNKNOWN* Unknown,
    _In_ REFCLSID RefClsId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);

NTSTATUS CreateMiniportWaveRTLamaLoopbackCapture(
    _Out_ PUNKNOWN* Unknown,
    _In_ REFCLSID RefClsId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);
// We are including that file at the top of this header, so we can use the class without redefining it

// Note: The original LamaLoopbackCaptureWaveFilterAutomation tables are removed
// as the KSPROPSETID_LamaLoopback is filter-wide and now handled by
// LamaLoopbackFilterAutomationTable applied directly to the filter descriptors.
// If wave-specific properties were needed, they would use their own automation table.
