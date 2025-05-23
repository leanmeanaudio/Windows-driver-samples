#pragma once

#include "lamaloopbackcommon.h"
#include "minipairs.h" 
#include "MiniportTopology.h" 
#include "MiniportWaveRT.h"   

// Forward declarations
class CMiniportTopologyLamaLoopbackCapture;
typedef CMiniportTopologyLamaLoopbackCapture *PCMiniportTopologyLamaLoopbackCapture;

class CMiniportWaveRTLamaLoopbackCapture;
typedef CMiniportWaveRTLamaLoopbackCapture *PCMiniportWaveRTLamaLoopbackCapture;

//=============================================================================
// Capture Topology Descriptors
//=============================================================================

//
// Capture Topology Pins
//
static
KSPIN_DESCRIPTOR_EX LamaLoopbackCaptureTopoPins[] =
{
    // KSPIN_TOPO_LOOPBACK_IN (Connects to Render Topology's Loopback Out)
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
            &KSCATEGORY_AUDIO,                  // Category (or a custom one like LAMA_LOOPBACK_BRIDGE_PIN_IN)
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_TOPO_LOOPBACK_IN              // Pin ID
    },
    // KSPIN_TOPO_BRIDGE_OUT (Connects to Wave's Host Pin)
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
            &KSCATEGORY_AUDIO,                  // Category
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_TOPO_BRIDGE_OUT               // Pin ID
    }
};

//
// Capture Topology Nodes (None for passthrough)
//
static
KSNODE_DESCRIPTOR LamaLoopbackCaptureTopoNodes[] =
{
    // No nodes for simple passthrough
};

//
// Capture Topology Connections
//
static
KSTOPOLOGY_CONNECTION LamaLoopbackCaptureTopoConnections[] =
{
    { KSPIN_TOPO_LOOPBACK_IN,   KSNODE_NONE,            KSPIN_TOPO_BRIDGE_OUT,  KSNODE_NONE }
};

//
// Capture Topology Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackCaptureTopologyFilterDescriptor =
{
    NULL,                                           // Dispatch
    NULL,                                           // AutomationTable
    KSFILTER_VERSION_DEVICE_SPECIFIC,               // Version
    0,                                              // Flags
    &KSCATEGORY_LAMA_LOOPBACK,                      // Categories (use the custom one)
    SIZEOF_ARRAY(LamaLoopbackCaptureTopoPins),      // PinDescriptorsCount
    LamaLoopbackCaptureTopoPins,                    // PinDescriptors
    SIZEOF_ARRAY(LamaLoopbackCaptureTopoNodes),     // NodeDescriptorsCount
    LamaLoopbackCaptureTopoNodes,                   // NodeDescriptors
    SIZEOF_ARRAY(LamaLoopbackCaptureTopoConnections),// ConnectionDescriptorsCount
    LamaLoopbackCaptureTopoConnections,             // ConnectionDescriptors
    NULL                                            // ComponentId
};

//=============================================================================
// Capture Wave Descriptors
//=============================================================================

//
// Capture Wave Pins
//
static
KSPIN_DESCRIPTOR_EX LamaLoopbackCaptureWavePins[] =
{
    // KSPIN_WAVE_BRIDGE_IN
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
            &LAMA_LOOPBACK_BRIDGE_PIN_OUT,      // Category (Specific bridge pin GUID)
            NULL,                               // Name
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          // Flags
        KSPIN_WAVE_BRIDGE_IN                // Pin ID
    },
    // KSPIN_WAVE_HOST_OUT
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
            KSPIN_COMMUNICATION_SINK,           // Communication
            &KSCATEGORY_AUDIO,                  // Category
            &PINNAME_LamaLoopbackWaveOut,       // Name GUID
            0                                   // ConstrainedDataRangesCount
        },
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING, // Flags
        KSPIN_WAVE_HOST_OUT                 // Pin ID
    }
};

//
// Capture Wave Nodes (None for passthrough)
//
static
KSNODE_DESCRIPTOR LamaLoopbackCaptureWaveNodes[] =
{
    // No nodes for simple passthrough
};

//
// Capture Wave Connections
//
static
KSTOPOLOGY_CONNECTION LamaLoopbackCaptureWaveConnections[] =
{
    { KSPIN_WAVE_BRIDGE_IN,    KSNODE_NONE,     KSPIN_WAVE_HOST_OUT,   KSNODE_NONE }
};

//
// Capture Wave Data Ranges (already defined in lamaloopbackcommon.h as PinDataRangesPcm)
//

//
// Capture Wave Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackCaptureWaveFilterDescriptor =
{
    NULL,                                           // Dispatch
    NULL,                                           // AutomationTable
    KSFILTER_VERSION_DEVICE_SPECIFIC,               // Version
    0,                                              // Flags
    &KSCATEGORY_AUDIO,                              // Categories
    SIZEOF_ARRAY(LamaLoopbackCaptureWavePins),      // PinDescriptorsCount
    LamaLoopbackCaptureWavePins,                    // PinDescriptors
    SIZEOF_ARRAY(LamaLoopbackCaptureWaveNodes),     // NodeDescriptorsCount
    LamaLoopbackCaptureWaveNodes,                   // NodeDescriptors
    SIZEOF_ARRAY(LamaLoopbackCaptureWaveConnections),// ConnectionDescriptorsCount
    LamaLoopbackCaptureWaveConnections,             // ConnectionDescriptors
    NULL                                            // ComponentId
};

//
// Capture Pin Device Formats and Modes
//
static
PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackCapturePinDeviceFormatsAndModes[] =
{
    {
        SystemCapturePin, // Pin ID (KSPIN_WAVE_HOST_OUT, assuming SystemCapturePin is defined as 0 or 1 in a common header)
        (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_16ch_16bit,
        NULL, // No anolog formats for this pin
        NULL, // No anolog formats for this pin
        MODE_RAW | MODE_DEFAULT, // Modes
        FALSE, // Modeless
        NULL   // Additional mode settings
    }
};

//
// Capture Miniport class
//
class CMiniportTopologyLamaLoopbackCapture : 
    public CMiniportTopology, // Inherits from CMiniportTopology in MiniportTopology.h (Sysvad)
    public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportTopologyLamaLoopbackCapture);
    ~CMiniportTopologyLamaLoopbackCapture();

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
// Capture Wave Filter Properties
//
static
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackCaptureWaveFilter, CMiniportWaveRT::PropertyHandler_WaveFilter);

//
// Capture Wave Filter Automation Table
//
DEFINE_PCAUTOMATION_TABLE_STD(LamaLoopbackCaptureWaveFilterAutomation, AutomationLamaLoopbackCaptureWaveFilter);

// Add KSPROPSETID_LamaLoopback manually if not covered by std handlers
static const PCPROPERTY_ITEM LamaLoopbackCaptureWaveProperties[] =
{
    {
        &KSPROPSETID_LamaLoopback,
        KSPROPERTY_LAMA_SAMPLE_RATE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportWaveRT::PropertyHandlerLamaSampleRate
    }
};

DEFINE_PCAUTOMATION_TABLE_APPEND_PROPERTIES(
    LamaLoopbackCaptureWaveFilterAutomationWithLama,
    LamaLoopbackCaptureWaveFilterAutomation,
    LamaLoopbackCaptureWaveProperties
);


//
// Capture Wave Miniport class
//
class CMiniportWaveRTLamaLoopbackCapture :
    public CMiniportWaveRT, // Inherits from CMiniportWaveRT in MiniportWaveRT.h (Sysvad)
    public CUnknown
{
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTLamaLoopbackCapture);
    ~CMiniportWaveRTLamaLoopbackCapture();

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
    // KSPIN_WAVE_BRIDGE_IN
    // KSPIN_WAVE_HOST_OUT
};
