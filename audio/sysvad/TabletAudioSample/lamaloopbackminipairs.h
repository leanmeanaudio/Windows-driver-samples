//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#ifndef _SYSVAD_LAMALOOPBACKMINIPAIRS_H_
#define _SYSVAD_LAMALOOPBACKMINIPAIRS_H_

#include "minipairs.h" // For ENDPOINT_MINIPAIR, MINIFILTER_DESCRIPTOR, PIN_DEVICE_FORMATS_AND_MODES, PHYSICALCONNECTIONTABLE, etc.
#include "lamaloopbacktoptable.h" // For Topology miniport filter descriptors & physical connections
#include "lamaloopbackwavtable.h" // For Wave miniport filter descriptors & pin formats/modes

// LAMA_LOOPBACK_MAX_CHANNELS is defined in lamaloopbackwavtable.h and used here.
// Ensure it's accessible. If not, #define LAMA_LOOPBACK_MAX_CHANNELS 16 here or ensure consistent definition.
// For this task, lamaloopbackwavtable.h defines it, and it's included above.

//=============================================================================
// Render Miniport Pair
//=============================================================================
static ENDPOINT_MINIPAIR LamaLoopbackRenderMiniports =
{
    // Topology Miniport
    {
        (MINIPORT_DEVICE_TYPE)0x100, // eLamaLoopbackRenderDevice - Placeholder
        L"TopologyLamaLoopbackRender",
        NULL,                               // PortFilterTemplateName
        CreateMiniportTopologySYSVAD,
        &LamaLoopbackRenderTopoMiniportFilterDescriptor, // From lamaloopbacktoptable.h
        0,                                  // InterfacePropertyCount
        NULL,                               // InterfacePropertyTables
    },
    // Wave Miniport
    {
        (MINIPORT_DEVICE_TYPE)0x100, // eLamaLoopbackRenderDevice - Placeholder
        L"WaveLamaLoopbackRender",
        NULL,                               // PortFilterTemplateName
        CreateMiniportWaveRTSYSVAD,
        &LamaLoopbackRenderWaveMiniportFilterDescriptor, // From lamaloopbackwavtable.h
        0,                                  // InterfacePropertyCount
        NULL,                               // InterfacePropertyTables
    },
    LAMA_LOOPBACK_MAX_CHANNELS, // From lamaloopbackwavtable.h
    LamaLoopback_PinDeviceFormatsAndModes, // From lamaloopbackwavtable.h
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes), // From lamaloopbackwavtable.h
    LamaLoopbackRenderTopologyPhysicalConnections, // From lamaloopbacktoptable.h
    SIZEOF_ARRAY(LamaLoopbackRenderTopologyPhysicalConnections),
    ENDPOINT_LOOPBACK_SUPPORTED, // DeviceFlags
    0,                                  // WaveModulesCount
    NULL,                               // WaveModules
    NULL                                // ModuleNotificationDeviceId
};

//=============================================================================
// Capture Miniport Pair
//=============================================================================
static ENDPOINT_MINIPAIR LamaLoopbackCaptureMiniports =
{
    // Topology Miniport
    {
        (MINIPORT_DEVICE_TYPE)0x101, // eLamaLoopbackCaptureDevice - Placeholder
        L"TopologyLamaLoopbackCapture",
        NULL,                               // PortFilterTemplateName
        CreateMiniportTopologySYSVAD,
        &LamaLoopbackCaptureTopoMiniportFilterDescriptor, // From lamaloopbacktoptable.h
        0,                                  // InterfacePropertyCount
        NULL,                               // InterfacePropertyTables
    },
    // Wave Miniport
    {
        (MINIPORT_DEVICE_TYPE)0x101, // eLamaLoopbackCaptureDevice - Placeholder
        L"WaveLamaLoopbackCapture",
        NULL,                               // PortFilterTemplateName
        CreateMiniportWaveRTSYSVAD,
        &LamaLoopbackCaptureWaveMiniportFilterDescriptor, // From lamaloopbackwavtable.h
        0,                                  // InterfacePropertyCount
        NULL,                               // InterfacePropertyTables
    },
    LAMA_LOOPBACK_MAX_CHANNELS, // From lamaloopbackwavtable.h
    LamaLoopback_PinDeviceFormatsAndModes, // From lamaloopbackwavtable.h
    SIZEOF_ARRAY(LamaLoopback_PinDeviceFormatsAndModes), // From lamaloopbackwavtable.h
    LamaLoopbackCaptureTopologyPhysicalConnections, // From lamaloopbacktoptable.h
    SIZEOF_ARRAY(LamaLoopbackCaptureTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,                  // DeviceFlags
    0,                                  // WaveModulesCount
    NULL,                               // WaveModules
    NULL                                // ModuleNotificationDeviceId
};

#endif // _SYSVAD_LAMALOOPBACKMINIPAIRS_H_
