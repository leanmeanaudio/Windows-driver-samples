#pragma once

#include "minipairs.h" 
#include "lamaloopbackrender.h"
#include "lamaloopbackcapture.h"
#include "baseaddress.h" // For eSpeakerDevice, eMicDevice etc. (assuming these are defined here or similar)

//=============================================================================
// Miniport Pair Definitions
//=============================================================================

//
// Factory functions for miniports (prototypes, implementation in .cpp files)
//
NTSTATUS CreateMiniportTopologyLamaLoopbackRender(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
);

NTSTATUS CreateMiniportWaveRTLamaLoopbackRender(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
);

NTSTATUS CreateMiniportTopologyLamaLoopbackCapture(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
);

NTSTATUS CreateMiniportWaveRTLamaLoopbackCapture(
    _Out_       PUNKNOWN *  Unknown,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN    UnknownOuter,
    _In_        POOL_TYPE   PoolType
);


//
// Render Miniport Pair
//
static
ENDPOINT_MINIPAIR LamaLoopbackRenderMiniportPair =
{
    eLamaLoopbackRenderDevice, // Placeholder, ensure this enum is defined (e.g. in baseaddress.h or a shared header)
    L"TopologyLamaLoopbackRender",                                  // TopologyName
    CreateMiniportTopologyLamaLoopbackRender,                       // TopologyFactory
    &LamaLoopbackRenderTopologyFilterDescriptor,                    // TopologyFilterDescriptor (from lamaloopbackrender.h)
    L"WaveLamaLoopbackRender",                                      // WaveName
    CreateMiniportWaveRTLamaLoopbackRender,                         // WaveFactory
    &LamaLoopbackRenderWaveFilterDescriptor,                        // WaveFilterDescriptor (from lamaloopbackrender.h)
    0,                                                              // Flags (e.g., ENDPOINT_FLAG_LOOPBACK_DEVICE if applicable)
    LamaLoopbackRenderPinDeviceFormatsAndModes,                     // PinDeviceFormatsAndModes (from lamaloopbackrender.h)
    SIZEOF_ARRAY(LamaLoopbackRenderPinDeviceFormatsAndModes),       // PinDeviceFormatsAndModesCount
    NULL,                                                           // Composite domaćin Resource Pointers
    0,                                                              // Composite domaćin Resource Count
    KSCATEGORY_AUDIO,                                               // Endpoint category
    0,                                                              // Endpoint flags
    Pcm48000_16ch_16bit.WaveFormatEx.nChannels                       // Number of channels
};

//
// Capture Miniport Pair
//
static
ENDPOINT_MINIPAIR LamaLoopbackCaptureMiniportPair =
{
    eLamaLoopbackCaptureDevice, // Placeholder, ensure this enum is defined
    L"TopologyLamaLoopbackCapture",                                 // TopologyName
    CreateMiniportTopologyLamaLoopbackCapture,                      // TopologyFactory
    &LamaLoopbackCaptureTopologyFilterDescriptor,                   // TopologyFilterDescriptor (from lamaloopbackcapture.h)
    L"WaveLamaLoopbackCapture",                                     // WaveName
    CreateMiniportWaveRTLamaLoopbackCapture,                        // WaveFactory
    &LamaLoopbackCaptureWaveFilterDescriptor,                       // WaveFilterDescriptor (from lamaloopbackcapture.h)
    0,                                                              // Flags
    LamaLoopbackCapturePinDeviceFormatsAndModes,                    // PinDeviceFormatsAndModes (from lamaloopbackcapture.h)
    SIZEOF_ARRAY(LamaLoopbackCapturePinDeviceFormatsAndModes),      // PinDeviceFormatsAndModesCount
    NULL,                                                           // Composite domaćin Resource Pointers
    0,                                                              // Composite domaćin Resource Count
    KSCATEGORY_AUDIO,                                               // Endpoint category
    0,                                                              // Endpoint flags
    Pcm48000_16ch_16bit.WaveFormatEx.nChannels                      // Number of channels
};


// Array of miniport pairs
// This will be used by the adapter to initialize the endpoints.
static
PENDPOINT_MINIPAIR LamaLoopbackMiniportPairs[] = 
{
    &LamaLoopbackRenderMiniportPair,
    &LamaLoopbackCaptureMiniportPair
};

// Count of miniport pairs
#define LAMA_LOOPBACK_MINIPAIR_COUNT (SIZEOF_ARRAY(LamaLoopbackMiniportPairs))

// Friendly names for the endpoints
#define LAMA_LOOPBACK_RENDER_FRIENDLY_NAME    L"Lama Loopback Render (16ch)"
#define LAMA_LOOPBACK_CAPTURE_FRIENDLY_NAME   L"Lama Loopback Capture (16ch)"

// Interface names (typically defined in the INF)
// #define LAMA_LOOPBACK_RENDER_INTERFACE_NAME   L"{GUID-RENDER-INTERFACE}" 
// #define LAMA_LOOPBACK_CAPTURE_INTERFACE_NAME  L"{GUID-CAPTURE-INTERFACE}"

// PnP IDs (typically defined in the INF)
// #define LAMA_LOOPBACK_RENDER_PNP_ID           L"YourCompanyLamaLoopbackRender"
// #define LAMA_LOOPBACK_CAPTURE_PNP_ID          L"YourCompanyLamaLoopbackCapture"
