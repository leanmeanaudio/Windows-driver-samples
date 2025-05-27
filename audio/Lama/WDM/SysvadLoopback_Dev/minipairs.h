#pragma once

#include "sysvad.h"
#include "common.h"
#include "lamaloopbackrender.h"
// Inline the contents of lamaloopbackcapture.h here to avoid include issues
// Begin lamaloopbackcapture.h contents
// Function declarations for Lama Loopback Capture functionality
NTSTATUS
CreateMiniportTopologyLamaLoopbackCapture(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID RefId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);

NTSTATUS
CreateMiniportWaveRTLamaLoopbackCapture(
    _Out_ PUNKNOWN *Unknown,
    _In_ REFCLSID RefId,
    _In_opt_ PUNKNOWN UnknownOuter,
    _In_ POOL_TYPE PoolType
);
// End lamaloopbackcapture.h contents

// Make sure we're using common.h which already has the ENDPOINT_MINIPAIR definition
#include "common.h"

// No need to redefine ENDPOINT_MINIPAIR as it's already defined in common.h
#if !defined(_SYSVAD_MINIPAIRS_H_REFERENCED_DEFINES)
#define _SYSVAD_MINIPAIRS_H_REFERENCED_DEFINES
#endif // _SYSVAD_MINIPAIRS_H_REFERENCED_DEFINES

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
    eLamaLoopbackRenderDevice,               // DeviceType
    
    // Topology miniport properties
    L"TopologyLamaLoopbackRender",          // TopoName
    NULL,                                   // TemplateTopoName (optional)
    CreateMiniportTopologyLamaLoopbackRender, // TopoCreateCallback
    &LamaLoopbackRenderTopologyFilterDescriptor, // TopoDescriptor
    0,                                      // TopoInterfacePropertyCount
    NULL,                                   // TopoInterfaceProperties
    
    // Wave RT miniport properties
    L"WaveLamaLoopbackRender",             // WaveName
    NULL,                                   // TemplateWaveName (optional)
    CreateMiniportWaveRTLamaLoopbackRender, // WaveCreateCallback
    &LamaLoopbackRenderWaveFilterDescriptor, // WaveDescriptor
    0,                                      // WaveInterfacePropertyCount
    NULL,                                   // WaveInterfaceProperties
    
    16,                                     // DeviceMaxChannels
    LamaLoopbackRenderPinDeviceFormatsAndModes, // PinDeviceFormatsAndModes
    SIZEOF_ARRAY(LamaLoopbackRenderPinDeviceFormatsAndModes), // PinDeviceFormatsAndModesCount
    
    // Physical connection properties
    NULL,                                   // PhysicalConnections
    0,                                      // PhysicalConnectionCount
    
    0,                                      // DeviceFlags
    
    // Module list properties
    NULL,                                   // ModuleList
    0,                                      // ModuleListCount
    NULL                                    // ModuleNotificationDeviceId
};

//
// Capture Miniport Pair
//
static
ENDPOINT_MINIPAIR LamaLoopbackCaptureMiniportPair =
{
    eLamaLoopbackCaptureDevice,                  // DeviceType
    
    // Topology miniport properties
    L"TopologyLamaLoopbackCapture",              // TopoName
    NULL,                                        // TemplateTopoName (optional)
    CreateMiniportTopologyLamaLoopbackCapture,   // TopoCreateCallback
    &LamaLoopbackCaptureTopologyFilterDescriptor, // TopoDescriptor
    0,                                           // TopoInterfacePropertyCount
    NULL,                                        // TopoInterfaceProperties
    
    // Wave RT miniport properties
    L"WaveLamaLoopbackCapture",                 // WaveName
    NULL,                                        // TemplateWaveName (optional)
    CreateMiniportWaveRTLamaLoopbackCapture,     // WaveCreateCallback
    &LamaLoopbackCaptureWaveFilterDescriptor,    // WaveDescriptor
    0,                                           // WaveInterfacePropertyCount
    NULL,                                        // WaveInterfaceProperties
    
    16,                                          // DeviceMaxChannels
    LamaLoopbackCapturePinDeviceFormatsAndModes, // PinDeviceFormatsAndModes
    SIZEOF_ARRAY(LamaLoopbackCapturePinDeviceFormatsAndModes), // PinDeviceFormatsAndModesCount
    
    // Physical connection properties
    NULL,                                        // PhysicalConnections
    0,                                           // PhysicalConnectionCount
    
    0,                                           // DeviceFlags
    
    // Module list properties
    NULL,                                        // ModuleList
    0,                                           // ModuleListCount
    NULL                                         // ModuleNotificationDeviceId
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
#ifndef LAMA_LOOPBACK_RENDER_FRIENDLY_NAME
#define LAMA_LOOPBACK_RENDER_FRIENDLY_NAME    L"Lama Loopback Render (16ch)"
#endif

#ifndef LAMA_LOOPBACK_CAPTURE_FRIENDLY_NAME
#define LAMA_LOOPBACK_CAPTURE_FRIENDLY_NAME   L"Lama Loopback Capture (16ch)"
#endif

// Interface names (typically defined in the INF)
// #define LAMA_LOOPBACK_RENDER_INTERFACE_NAME   L"{GUID-RENDER-INTERFACE}" 
// #define LAMA_LOOPBACK_CAPTURE_INTERFACE_NAME  L"{GUID-CAPTURE-INTERFACE}"

// PnP IDs (typically defined in the INF)
// #define LAMA_LOOPBACK_RENDER_PNP_ID           L"YourCompanyLamaLoopbackRender"
// #define LAMA_LOOPBACK_CAPTURE_PNP_ID          L"YourCompanyLamaLoopbackCapture"
