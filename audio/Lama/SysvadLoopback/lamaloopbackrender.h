#pragma once

#include "lamaloopbackcommon.h"
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
        NULL,                               // AutomationTable
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
    // No nodes for simple passthrough
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
    NULL,                                           // Dispatch
    NULL,                                           // AutomationTable
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
        NULL,                               // AutomationTable
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
// Render Wave Nodes (None for passthrough)
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
// Render Wave Data Ranges (already defined in lamaloopbackcommon.h as PinDataRangesPcm)
//

//
// Render Wave Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackRenderWaveFilterDescriptor =
{
    NULL,                                           // Dispatch
    NULL,                                           // AutomationTable
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
//
static
PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackRenderPinDeviceFormatsAndModes[] =
{
    {
        SystemRenderPin, // Pin ID (KSPIN_WAVE_HOST_IN, assuming SystemRenderPin is defined as 0 in a common header like baseaddress.h or similar)
        (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_16ch_16bit,
        NULL, // No anolog formats for this pin
        NULL, // No anolog formats for this pin
        MODE_RAW | MODE_DEFAULT, // Modes
        FALSE, // Modeless
        NULL   // Additional mode settings
    }
};

//
// Render Miniport class
//
class CMiniportTopologyLamaLoopbackRender : 
    public CMiniportTopology, // Inherits from CMiniportTopology in MiniportTopology.h (Sysvad)
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

    // DataRange intersection handler
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
// Render Wave Filter Properties
//
static
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackRenderWaveFilter, CMiniportWaveRT::PropertyHandler_WaveFilter);

//
// Render Wave Filter Automation Table
//
DEFINE_PCAUTOMATION_TABLE_STD(LamaLoopbackRenderWaveFilterAutomation, AutomationLamaLoopbackRenderWaveFilter);
// Add KSPROPSETID_LamaLoopback manually if not covered by std handlers
static const PCPROPERTY_ITEM LamaLoopbackRenderWaveProperties[] =
{
    {
        &KSPROPSETID_LamaLoopback,
        KSPROPERTY_LAMA_SAMPLE_RATE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportWaveRT::PropertyHandlerLamaSampleRate
    }
};

DEFINE_PCAUTOMATION_TABLE_APPEND_PROPERTIES(
    LamaLoopbackRenderWaveFilterAutomationWithLama,
    LamaLoopbackRenderWaveFilterAutomation,
    LamaLoopbackRenderWaveProperties
);


//
// Render Wave Miniport class
//
class CMiniportWaveRTLamaLoopbackRender :
    public CMiniportWaveRT, // Inherits from CMiniportWaveRT in MiniportWaveRT.h (Sysvad)
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

    // Pins (already defined in lamaloopbackcommon.h)
    // KSPIN_WAVE_HOST_IN
    // KSPIN_WAVE_BRIDGE_OUT
};
