#pragma once

#include "lamaloopbackcommon.h" // For LamaLoopbackFilterAutomationTable extern declaration
#include "minipairs.h" 
#include "MiniportTopology.h" 
#include "MiniportWaveRT.h"   

// Forward declarations
class CMiniportTopologyLamaLoopbackRender;
typedef CMiniportTopologyLamaLoopbackRender *PCMiniportTopologyLamaLoopbackRender;

class CMiniportWaveRTLamaLoopbackRender;
typedef CMiniportWaveRTLamaLoopbackRender *PCMiniportWaveRTLamaLoopbackRender;

//=============================================================================
// Render Topology Descriptors
//=============================================================================

//
// Render Topology Pins
//
static
KSPIN_DESCRIPTOR_EX LamaLoopbackRenderTopoPins[] =
{
    // KSPIN_TOPO_BRIDGE_IN (Connects to Wave's Host Pin)
    {
        NULL,                               // Dispatch
        NULL,                               // AutomationTable (Pins usually don't have their own top-level automation table)
        {                                   // PinDesc
            0,                                  // InterfacesCount
            NULL,                               // Interfaces
            0,                                  // MediumsCount
            NULL,                               // Mediums
            SIZEOF_ARRAY(PinDataRangesPcm),     // DataRangesCount
            PinDataRangesPcm,                   // DataRanges
            KSPIN_DATAFLOW_IN,                  // DataFlow
            KSPIN_COMMUNICATION_NONE,           // Communication
            &KSCATEGORY_AUDIO,                  // Category
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_TOPO_BRIDGE_IN                // Pin ID
    },
    // KSPIN_TOPO_LOOPBACK_OUT (Connects to Capture Topology's Loopback In)
    {
        NULL,                               // Dispatch
        NULL,                               // AutomationTable
        {                                   // PinDesc
            0,                                  // InterfacesCount
            NULL,                               // Interfaces
            0,                                  // MediumsCount
            NULL,                               // Mediums
            SIZEOF_ARRAY(PinDataRangesPcm),     // DataRangesCount
            PinDataRangesPcm,                   // DataRanges
            KSPIN_DATAFLOW_OUT,                 // DataFlow
            KSPIN_COMMUNICATION_NONE,           // Communication
            &KSCATEGORY_AUDIO,                  // Category (or a custom one like LAMA_LOOPBACK_BRIDGE_PIN_OUT)
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_TOPO_LOOPBACK_OUT             // Pin ID
    }
};

//
// Render Topology Nodes (None for passthrough)
//
static
KSNODE_DESCRIPTOR LamaLoopbackRenderTopoNodes[] =
{
    // No nodes for simple passthrough, but could include AEC, Mute, Volume here if desired
    // For KSPROPSETID_LamaLoopback, it's a filter-level property, not node.
};

//
// Render Topology Connections
//
static
KSTOPOLOGY_CONNECTION LamaLoopbackRenderTopoConnections[] =
{
    { KSPIN_TOPO_BRIDGE_IN,     KSNODE_NONE,            KSPIN_TOPO_LOOPBACK_OUT,    KSNODE_NONE }
};

//
// Render Topology Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackRenderTopologyFilterDescriptor =
{
    NULL,                                           // Dispatch (PortCls handles this for topology)
    &LamaLoopbackFilterAutomationTable,             // AutomationTable <--- MODIFIED
    KSFILTER_VERSION_DEVICE_SPECIFIC,               // Version
    0,                                              // Flags
    &KSCATEGORY_LAMA_LOOPBACK,                      // Categories (use the custom one)
    SIZEOF_ARRAY(LamaLoopbackRenderTopoPins),       // PinDescriptorsCount
    LamaLoopbackRenderTopoPins,                     // PinDescriptors
    SIZEOF_ARRAY(LamaLoopbackRenderTopoNodes),      // NodeDescriptorsCount
    LamaLoopbackRenderTopoNodes,                    // NodeDescriptors
    SIZEOF_ARRAY(LamaLoopbackRenderTopoConnections),// ConnectionDescriptorsCount
    LamaLoopbackRenderTopoConnections,              // ConnectionDescriptors
    NULL                                            // ComponentId
};

//=============================================================================
// Render Wave Descriptors
//=============================================================================

//
// Render Wave Pins
//
static
KSPIN_DESCRIPTOR_EX LamaLoopbackRenderWavePins[] =
{
    // KSPIN_WAVE_HOST_IN
    {
        NULL,                               // Dispatch
        NULL,                               // AutomationTable (Pins typically don't have one here)
        {                                   // PinDesc
            0,                                  // InterfacesCount
            NULL,                               // Interfaces
            0,                                  // MediumsCount
            NULL,                               // Mediums
            SIZEOF_ARRAY(PinDataRangesPcm),     // DataRangesCount
            PinDataRangesPcm,                   // DataRanges
            KSPIN_DATAFLOW_IN,                  // DataFlow
            KSPIN_COMMUNICATION_SINK,           // Communication
            &KSCATEGORY_AUDIO,                  // Category
            &PINNAME_LamaLoopbackWaveIn,        // Name GUID
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING, // Flags
        KSPIN_WAVE_HOST_IN                  // Pin ID
    },
    // KSPIN_WAVE_BRIDGE_OUT
    {
        NULL,                               // Dispatch
        NULL,                               // AutomationTable
        {                                   // PinDesc
            0,                                  // InterfacesCount
            NULL,                               // Interfaces
            0,                                  // MediumsCount
            NULL,                               // Mediums
            SIZEOF_ARRAY(PinDataRangesPcm),     // DataRangesCount
            PinDataRangesPcm,                   // DataRanges
            KSPIN_DATAFLOW_OUT,                 // DataFlow
            KSPIN_COMMUNICATION_NONE,           // Communication
            &LAMA_LOOPBACK_BRIDGE_PIN_IN,       // Category (Specific bridge pin GUID)
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_WAVE_BRIDGE_OUT               // Pin ID
    }
};

//
// Render Wave Nodes (None for passthrough, but could have a SRC node if sample rate conversion was supported independently of global rate)
//
static
KSNODE_DESCRIPTOR LamaLoopbackRenderWaveNodes[] =
{
    // No nodes for simple passthrough
};

//
// Render Wave Connections
//
static
KSTOPOLOGY_CONNECTION LamaLoopbackRenderWaveConnections[] =
{
    { KSPIN_WAVE_HOST_IN,       KSNODE_NONE,    KSPIN_WAVE_BRIDGE_OUT,  KSNODE_NONE }
};

//
// Render Wave Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackRenderWaveFilterDescriptor =
{
    NULL,                                           // Dispatch (PortCls handles for wave too)
    &LamaLoopbackFilterAutomationTable,             // AutomationTable <--- MODIFIED
    KSFILTER_VERSION_DEVICE_SPECIFIC,               // Version
    0,                                              // Flags
    &KSCATEGORY_AUDIO,                              // Categories
    SIZEOF_ARRAY(LamaLoopbackRenderWavePins),       // PinDescriptorsCount
    LamaLoopbackRenderWavePins,                     // PinDescriptors
    SIZEOF_ARRAY(LamaLoopbackRenderWaveNodes),      // NodeDescriptorsCount
    LamaLoopbackRenderWaveNodes,                    // NodeDescriptors
    SIZEOF_ARRAY(LamaLoopbackRenderWaveConnections),// ConnectionDescriptorsCount
    LamaLoopbackRenderWaveConnections,              // ConnectionDescriptors
    NULL                                            // ComponentId
};

//
// Render Pin Device Formats and Modes
// (This structure is typically used by IPortClsStreamResourceManager)
//
static
PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackRenderPinDeviceFormatsAndModes[] =
{
    {
        SystemRenderPin, // Pin ID (KSPIN_WAVE_HOST_IN, assuming SystemRenderPin is defined as 0)
        (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_Stereo_16bit, // Default format, DataRangeIntersection will handle current global settings
        NULL, 
        NULL, 
        MODE_RAW | MODE_DEFAULT, 
        FALSE, 
        NULL   
    }
};

//
// Render Miniport class (Topology)
//
class CMiniportTopologyLamaLoopbackRender : 
    public CMiniportTopology, 
    public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportTopologyLamaLoopbackRender);
    ~CMiniportTopologyLamaLoopbackRender();

    NTSTATUS                Init
    (
        _In_  PUNKNOWN        UnknownAdapter,
        _In_  PRESOURCELIST   ResourceList,
        _In_  PPORTTOPOLOGY   PortTopology
    );

    NTSTATUS                DataRangeIntersection
    (
        _In_        ULONG           PinId,
        _In_        PKSDATARANGE    DataRange,
        _In_        PKSDATARANGE    MatchingDataRange,
        _In_        ULONG           OutputBufferLength,
        _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                    PVOID           ResultantFormat,
        _Out_       PULONG          ResultantFormatLength
    );

private:
    PPORTTOPOLOGY           m_Port;
    PUNKNOWN                m_UnknownAdapter;
};

//
// Render Miniport class (WaveRT)
//
class CMiniportWaveRTLamaLoopbackRender :
    public CMiniportWaveRT, 
    public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTLamaLoopbackRender);
    ~CMiniportWaveRTLamaLoopbackRender();

    NTSTATUS                Init
    (
        _In_  PUNKNOWN        UnknownAdapter,
        _In_  PRESOURCELIST   ResourceList,
        _In_  PPORTWAVERT     Port
    );

    NTSTATUS                NewStream
    (
        _Out_ PMINIPORTWAVERTSTREAM * Stream,
        _In_  PPORTWAVERTSTREAM       PortStream,
        _In_  ULONG                   Pin,
        _In_  BOOLEAN                 Capture,
        _In_  PKSDATAFORMAT           DataFormat
    );

private:
    PPORTWAVERT             m_Port;
    PUNKNOWN                m_UnknownAdapter;
};

// Note: The original LamaLoopbackRenderWaveFilterAutomation tables defined here
// were specific to CMiniportWaveRT::PropertyHandler_WaveFilter and CMiniportWaveRT::PropertyHandlerLamaSampleRate.
// The new KSPROPSETID_LamaLoopback is filter-wide and defined via LamaLoopbackFilterAutomationTable.
// If additional wave-specific properties were needed, they would use a separate automation table.
// For this task, LamaLoopbackFilterAutomationTable is applied to the filter descriptor.Okay, I've updated `lamaloopbackcommon.h` and `lamaloopbackrender.h`.

Now, I will proceed to **Step 5.3: Update `PCFILTER_DESCRIPTOR` in `lamaloopbackcapture.h`**.

I need to:
1.  Read `lamaloopbackcapture.h`.
2.  Locate the `PCFILTER_DESCRIPTOR` definitions for the capture topology and wave filters.
3.  Set their `.AutomationTable = &LamaLoopbackFilterAutomationTable;` (which is declared `extern` in `lamaloopbackcommon.h` and defined in `lamaloopbackrender.cpp`).
4.  Overwrite the file.
