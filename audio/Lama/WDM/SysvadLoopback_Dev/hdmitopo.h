#pragma once
#ifndef _HDMITOPO_H_
#define _HDMITOPO_H_

#include "sysvad.h"
#include "common.h"
#include "minipairs.h"
#include "lamaloopbackcommon.h" // For LAMA_POOL_TAG, MIN/MAX_SAMPLE_RATE_PCM, LAMA_MAX_BUFFER_SIZE etc.

#ifndef MAX_CHANNELS
#define MAX_CHANNELS 16 // Default max channels if not in common.h
#endif

//=============================================================================
// Forward declarations for classes that will be defined in the implementation
//=============================================================================

// Forward declare the stream class - full implementation will be in .cpp file
class CMiniportWaveRTLamaLoopbackStream;

// Define a pointer type for the class
typedef CMiniportWaveRTLamaLoopbackStream* PCMiniportWaveRTLamaLoopbackStream;

// The class implementation is defined in the .cpp file
// Methods for the CMiniportWaveRTLamaLoopbackStream class will be implemented there

// Declaration of a helper function to get hardware description
// This function is implemented in hdmitopo.cpp
NTSTATUS GetMinipairHardwareDescription(
    _Out_    PENDPOINT_MINIPAIR    Minipair,
    _Outptr_result_maybenull_ PVOID*       DeviceContext
);
// No implementation here - moved to hdmitopo.cpp

// Forward declarations for any additional helper functions can be added here

#endif // _HDMITOPO_H_
