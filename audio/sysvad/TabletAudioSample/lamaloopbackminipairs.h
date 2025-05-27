#include "lamawdkcompat.h" // Include this first for WDK compatibility fixes

//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKMINIPAIRS_H_
#define _SYSVAD_LAMALOOPBACKMINIPAIRS_H_

// Removed: #include "minipairs.h" 

// Added direct includes:
#include "sysvad.h"     // For ENDPOINT_MINIPAIR, MINIFILTER_DESCRIPTOR, PHYSICALCONNECTIONTABLE, eDeviceType, CONNECTIONTYPE, PENDPOINT_MINIPAIR, etc.
#include "..\EndpointsCommon\minwavert.h" // For PIN_DEVICE_FORMATS_AND_MODES

// Fix for PWSTR type mismatch error
#ifndef PWSTR
typedef WCHAR* PWSTR;
#endif

#include "lamaloopbacktoptable.h" // For Topology miniport filter descriptors & physical connections
#include "lamaloopbackwavtable.h" // For Wave miniport filter descriptors & pin formats/modes

// LAMA_LOOPBACK_MAX_CHANNELS is defined in lamaloopbackwavtable.h and used here.
// Ensure it's accessible. If not, #define LAMA_LOOPBACK_MAX_CHANNELS 16 here or ensure consistent definition.
// For this task, lamaloopbackwavtable.h defines it, and it's included above.

//=============================================================================
// Render Miniport Pair
//=============================================================================
// Define custom device types to avoid eDeviceType casting issues
#define LAMA_LOOPBACK_RENDER_DEVICE  0x100
#define LAMA_LOOPBACK_CAPTURE_DEVICE 0x101

static ENDPOINT_MINIPAIR LamaLoopbackRenderMiniports =
{
    (eDeviceType)LAMA_LOOPBACK_RENDER_DEVICE,       // DeviceType
    // Topology
    L"TopologyLamaLoopbackRender",                 // TopoName
    nullptr,                                        // TemplateTopoName
    (PFNCREATEMINIPORT)CreateMiniportTopologySYSVAD,// TopoCreateCallback
    (PCFILTER_DESCRIPTOR*)&LamaLoopbackRenderTopoMiniportFilterDescriptor, // TopoDescriptor (cast from MINIFILTER_DESCRIPTOR*)
    0,                                              // TopoInterfacePropertyCount
    nullptr,                                        // TopoInterfaceProperties
    // Wave
    L"WaveLamaLoopbackRender",                     // WaveName
    nullptr,                                        // TemplateWaveName
    (PFNCREATEMINIPORT)CreateMiniportWaveRTSYSVAD,   // WaveCreateCallback
    (PCFILTER_DESCRIPTOR*)&LamaLoopbackRenderWaveMiniportFilterDescriptor, // WaveDescriptor (cast from MINIFILTER_DESCRIPTOR*)
    0,                                              // WaveInterfacePropertyCount
    nullptr,                                        // WaveInterfaceProperties

    LAMA_LOOPBACK_MAX_CHANNELS,                     // DeviceMaxChannels (from lamaloopbackwavtable.h)
    LamaLoopback_PinDeviceFormatsAndModes,          // PinDeviceFormatsAndModes (from lamaloopbackwavtable.h)
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes), // PinDeviceFormatsAndModesCount
    nullptr,                                        // PhysicalConnections
    0,                                              // PhysicalConnectionCount
    ENDPOINT_LOOPBACK_SUPPORTED,                    // DeviceFlags
    nullptr,                                        // ModuleList
    0,                                              // ModuleListCount
    nullptr                                         // ModuleNotificationDeviceId
};

//=============================================================================
// Capture Miniport Pair
//=============================================================================
static ENDPOINT_MINIPAIR LamaLoopbackCaptureMiniports =
{
    (eDeviceType)LAMA_LOOPBACK_CAPTURE_DEVICE,      // DeviceType
    // Topology
    L"TopologyLamaLoopbackCapture",                // TopoName
    nullptr,                                        // TemplateTopoName
    (PFNCREATEMINIPORT)CreateMiniportTopologySYSVAD,// TopoCreateCallback
    (PCFILTER_DESCRIPTOR*)&LamaLoopbackCaptureTopoMiniportFilterDescriptor, // TopoDescriptor (cast from MINIFILTER_DESCRIPTOR*)
    0,                                              // TopoInterfacePropertyCount
    nullptr,                                        // TopoInterfaceProperties
    // Wave
    L"WaveLamaLoopbackCapture",                    // WaveName
    nullptr,                                        // TemplateWaveName
    (PFNCREATEMINIPORT)CreateMiniportWaveRTSYSVAD,   // WaveCreateCallback
    (PCFILTER_DESCRIPTOR*)&LamaLoopbackCaptureWaveMiniportFilterDescriptor, // WaveDescriptor (cast from MINIFILTER_DESCRIPTOR*)
    0,                                              // WaveInterfacePropertyCount
    nullptr,                                        // WaveInterfaceProperties

    LAMA_LOOPBACK_MAX_CHANNELS,                     // DeviceMaxChannels (from lamaloopbackwavtable.h)
    LamaLoopback_PinDeviceFormatsAndModes,          // PinDeviceFormatsAndModes (from lamaloopbackwavtable.h)
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes), // PinDeviceFormatsAndModesCount
    nullptr,                                        // PhysicalConnections
    0,                                              // PhysicalConnectionCount
    ENDPOINT_NO_FLAGS,          // DeviceFlags
    nullptr,                                        // ModuleList
    0,                                              // ModuleListCount
    nullptr                                         // ModuleNotificationDeviceId
};

#endif // _SYSVAD_LAMALOOPBACKMINIPAIRS_H_
