#pragma once

#include "lamaloopbackcommon.h" // For LamaLoopbackFilterAutomationTable extern declaration
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
            &KSCATEGORY_AUDIO,                  // Category (or LAMA_LOOPBACK_BRIDGE_PIN_IN)
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
// Capture Topology Nodes
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
    &LamaLoopbackFilterAutomationTable,             // AutomationTable <--- MODIFIED
    KSFILTER_VERSION_DEVICE_SPECIFIC,               // Version
    0,                                              // Flags
    &KSCATEGORY_LAMA_LOOPBACK,                      // Categories
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
// Capture Wave Nodes
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
// Capture Wave Filter Descriptor
//
static
KSFILTER_DESCRIPTOR LamaLoopbackCaptureWaveFilterDescriptor =
{
    NULL,                                           // Dispatch
    &LamaLoopbackFilterAutomationTable,             // AutomationTable <--- MODIFIED
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
        SystemCapturePin, // Pin ID (KSPIN_WAVE_HOST_OUT)
        (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_Stereo_16bit, // Default format
        NULL, 
        NULL, 
        MODE_RAW | MODE_DEFAULT, 
        FALSE, 
        NULL   
    }
};

//
// Capture Miniport class (Topology)
//
class CMiniportTopologyLamaLoopbackCapture : 
    public CMiniportTopology, 
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
// Capture Miniport class (WaveRT)
//
class CMiniportWaveRTLamaLoopbackCapture :
    public CMiniportWaveRT, 
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
};

// Note: The original LamaLoopbackCaptureWaveFilterAutomation tables are removed
// as the KSPROPSETID_LamaLoopback is filter-wide and now handled by
// LamaLoopbackFilterAutomationTable applied directly to the filter descriptors.
// If wave-specific properties were needed, they would use their own automation table.
