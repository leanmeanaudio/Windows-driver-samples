#pragma once

#include "lamaloopbackcommon.h" // For LamaLoopbackFilterAutomationTable extern declaration, PinDataRangesPcm, GUIDs etc.
#include "minipairs.h" 
#include "MiniportTopology.h" 
#include "MiniportWaveRT.h"   

// Forward declarations for Render
class CMiniportTopologyLamaLoopbackRender;
typedef CMiniportTopologyLamaLoopbackRender *PCMiniportTopologyLamaLoopbackRender;

class CMiniportWaveRTLamaLoopbackRender;
typedef CMiniportWaveRTLamaLoopbackRender *PCMiniportWaveRTLamaLoopbackRender;

NTSTATUS LamaRenderPinWrite(_In_ PKSPIN Pin, _In_ PIRP Irp);

// Forward declarations for Capture
class CMiniportTopologyLamaLoopbackCapture;
typedef CMiniportTopologyLamaLoopbackCapture *PCMiniportTopologyLamaLoopbackCapture;

class CMiniportWaveRTLamaLoopbackCapture;
typedef CMiniportWaveRTLamaLoopbackCapture *PCMiniportWaveRTLamaLoopbackCapture;

NTSTATUS LamaCapturePinRead(_In_ PKSPIN Pin, _In_ PIRP Irp);


//=============================================================================
// Render Topology Descriptors
//=============================================================================
static KSPIN_DESCRIPTOR_EX LamaLoopbackRenderTopoPins[] =
{
    // KSPIN_TOPO_BRIDGE_IN (ID 0) (Connects to Wave's Host Pin)
    {
        NULL,                               
        NULL,                               
        {                                   
            0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm,
            KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE,
            &LAMA_LOOPBACK_BRIDGE_PIN_IN, // Category: Matches Render Wave's KSPIN_WAVE_BRIDGE_OUT category
            NULL, 0                                   
        },
        KSPIN_FLAG_DISPATCH_LEVEL,         
        KSPIN_TOPO_BRIDGE_IN                
    },
    // KSPIN_TOPO_LOOPBACK_OUT (ID 1) (Connects to Capture Topology's Loopback In)
    {
        NULL,                               
        NULL,                               
        {                                   
            0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm,                   
            KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE,           
            &KSCATEGORY_AUDIO, // Category for inter-topology loopback connection         
            NULL, 0                                   
        },
        KSPIN_FLAG_DISPATCH_LEVEL,          
        KSPIN_TOPO_LOOPBACK_OUT             
    }
};

static KSNODE_DESCRIPTOR LamaLoopbackRenderTopoNodes[] = { /* None */ };
static KSTOPOLOGY_CONNECTION LamaLoopbackRenderTopoConnections[] =
{
    { KSPIN_TOPO_BRIDGE_IN, KSNODE_NONE, KSPIN_TOPO_LOOPBACK_OUT, KSNODE_NONE }
};

static KSFILTER_DESCRIPTOR LamaLoopbackRenderTopologyFilterDescriptor =
{
    NULL, &LamaLoopbackFilterAutomationTable, KSFILTER_VERSION_DEVICE_SPECIFIC, 0,
    &KSCATEGORY_LAMA_LOOPBACK, SIZEOF_ARRAY(LamaLoopbackRenderTopoPins), LamaLoopbackRenderTopoPins,
    SIZEOF_ARRAY(LamaLoopbackRenderTopoNodes), LamaLoopbackRenderTopoNodes,
    SIZEOF_ARRAY(LamaLoopbackRenderTopoConnections), LamaLoopbackRenderTopoConnections, NULL
};

//=============================================================================
// Render Wave Descriptors
//=============================================================================
static const KSPIN_DISPATCH LamaRenderPinDispatch =
{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LamaRenderPinWrite, NULL, NULL, NULL, NULL, NULL };

static KSPIN_DESCRIPTOR_EX LamaLoopbackRenderWavePins[] =
{
    // KSPIN_WAVE_HOST_IN (ID 0)
    {
        &LamaRenderPinDispatch, NULL,                               
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_SINK, &KSCATEGORY_AUDIO, &PINNAME_LamaLoopbackWaveIn, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING, KSPIN_WAVE_HOST_IN
    },
    // KSPIN_WAVE_BRIDGE_OUT (ID 1)
    {
        NULL, NULL,                               
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &LAMA_LOOPBACK_BRIDGE_PIN_IN, NULL, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL, KSPIN_WAVE_BRIDGE_OUT
    }
};

static KSNODE_DESCRIPTOR LamaLoopbackRenderWaveNodes[] = { /* None */ };
static KSTOPOLOGY_CONNECTION LamaLoopbackRenderWaveConnections[] =
{
    { KSPIN_WAVE_HOST_IN, KSNODE_NONE, KSPIN_WAVE_BRIDGE_OUT, KSNODE_NONE }
};

static KSFILTER_DESCRIPTOR LamaLoopbackRenderWaveFilterDescriptor =
{
    NULL, &LamaLoopbackFilterAutomationTable, KSFILTER_VERSION_DEVICE_SPECIFIC, 0,
    &KSCATEGORY_AUDIO, SIZEOF_ARRAY(LamaLoopbackRenderWavePins), LamaLoopbackRenderWavePins,
    SIZEOF_ARRAY(LamaLoopbackRenderWaveNodes), LamaLoopbackRenderWaveNodes,
    SIZEOF_ARRAY(LamaLoopbackRenderWaveConnections), LamaLoopbackRenderWaveConnections, NULL
};

static PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackRenderPinDeviceFormatsAndModes[] =
{ { KSPIN_WAVE_HOST_IN, (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_Stereo_16bit, NULL, NULL, MODE_RAW | MODE_DEFAULT, FALSE, NULL } };

//=============================================================================
// Render Miniport Class Definitions
//=============================================================================
class CMiniportTopologyLamaLoopbackRender : public CMiniportTopology, public CUnknown 
{ 
public: DECLARE_STD_UNKNOWN(); DEFINE_STD_CONSTRUCTOR(CMiniportTopologyLamaLoopbackRender); ~CMiniportTopologyLamaLoopbackRender();
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY PortTopology);
    NTSTATUS DataRangeIntersection(_In_ ULONG PinId, _In_ PKSDATARANGE DataRange, _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength, _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat, _Out_ PULONG ResultantFormatLength);
private: PPORTTOPOLOGY m_Port; PUNKNOWN m_UnknownAdapter;
};

class CMiniportWaveRTLamaLoopbackRender : public CMiniportWaveRT, public CUnknown 
{ 
public: DECLARE_STD_UNKNOWN(); DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTLamaLoopbackRender); ~CMiniportWaveRTLamaLoopbackRender();
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port);
    NTSTATUS NewStream(_Out_ PMINIPORTWAVERTSTREAM * Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat);
private: 
    PPORTWAVERT m_Port; 
    PUNKNOWN m_UnknownAdapter;
    ULONG m_MiniportInstanceIndex; // Added instance index
};

//=============================================================================
// Capture Topology Descriptors
//=============================================================================
static KSPIN_DESCRIPTOR_EX LamaLoopbackCaptureTopoPins[] =
{
    // KSPIN_TOPO_LOOPBACK_IN (ID 0)
    {
        NULL, NULL,
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, NULL, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL, KSPIN_TOPO_LOOPBACK_IN
    },
    // KSPIN_TOPO_BRIDGE_OUT (ID 1)
    {
        NULL, NULL,
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, NULL, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL, KSPIN_TOPO_BRIDGE_OUT
    }
};

static KSNODE_DESCRIPTOR LamaLoopbackCaptureTopoNodes[] = { /* None */ };
static KSTOPOLOGY_CONNECTION LamaLoopbackCaptureTopoConnections[] =
{ { KSPIN_TOPO_LOOPBACK_IN, KSNODE_NONE, KSPIN_TOPO_BRIDGE_OUT, KSNODE_NONE } };

static KSFILTER_DESCRIPTOR LamaLoopbackCaptureTopologyFilterDescriptor =
{
    NULL, &LamaLoopbackFilterAutomationTable, KSFILTER_VERSION_DEVICE_SPECIFIC, 0,
    &KSCATEGORY_LAMA_LOOPBACK, SIZEOF_ARRAY(LamaLoopbackCaptureTopoPins), LamaLoopbackCaptureTopoPins,
    SIZEOF_ARRAY(LamaLoopbackCaptureTopoNodes), LamaLoopbackCaptureTopoNodes,
    SIZEOF_ARRAY(LamaLoopbackCaptureTopoConnections), LamaLoopbackCaptureTopoConnections, NULL
};

//=============================================================================
// Capture Wave Descriptors
//=============================================================================
static const KSPIN_DISPATCH LamaCapturePinDispatch =
{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LamaCapturePinRead, NULL, NULL, NULL, NULL };

static KSPIN_DESCRIPTOR_EX LamaLoopbackCaptureWavePins[] =
{
    // KSPIN_WAVE_CAPTURE_HOST_OUT (ID 0)
    {
        &LamaCapturePinDispatch, NULL,
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_SOURCE, &KSCATEGORY_AUDIO, &PINNAME_LamaLoopbackWaveOut, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING, KSPIN_WAVE_CAPTURE_HOST_OUT
    },
    // KSPIN_WAVE_BRIDGE_IN (ID 1)
    {
        NULL, NULL,
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE, &KSCATEGORY_AUDIO, NULL, 0 },
        KSPIN_FLAG_DISPATCH_LEVEL, KSPIN_WAVE_BRIDGE_IN
    }
};

static KSNODE_DESCRIPTOR LamaLoopbackCaptureWaveNodes[] = { /* None */ };
static KSTOPOLOGY_CONNECTION LamaLoopbackCaptureWaveConnections[] =
{ { KSPIN_WAVE_BRIDGE_IN, KSNODE_NONE, KSPIN_WAVE_CAPTURE_HOST_OUT, KSNODE_NONE } };

static KSFILTER_DESCRIPTOR LamaLoopbackCaptureWaveFilterDescriptor =
{
    NULL, &LamaLoopbackFilterAutomationTable, KSFILTER_VERSION_DEVICE_SPECIFIC, 0,
    &KSCATEGORY_AUDIO, SIZEOF_ARRAY(LamaLoopbackCaptureWavePins), LamaLoopbackCaptureWavePins,
    SIZEOF_ARRAY(LamaLoopbackCaptureWaveNodes), LamaLoopbackCaptureWaveNodes,
    SIZEOF_ARRAY(LamaLoopbackCaptureWaveConnections), LamaLoopbackCaptureWaveConnections, NULL
};

static PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackCapturePinDeviceFormatsAndModes[] =
{ { KSPIN_WAVE_CAPTURE_HOST_OUT, (PKSDATAFORMAT_WAVEFORMATEXTENSIBLE)&Pcm48000_Stereo_16bit, NULL, NULL, MODE_RAW | MODE_DEFAULT, FALSE, NULL } };

//=============================================================================
// Capture Miniport Class Definitions
//=============================================================================
class CMiniportTopologyLamaLoopbackCapture : public CMiniportTopology, public CUnknown
{
public: DECLARE_STD_UNKNOWN(); DEFINE_STD_CONSTRUCTOR(CMiniportTopologyLamaLoopbackCapture); ~CMiniportTopologyLamaLoopbackCapture();
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY PortTopology);
    NTSTATUS DataRangeIntersection(_In_ ULONG PinId, _In_ PKSDATARANGE DataRange, _In_ PKSDATARANGE MatchingDataRange, _In_ ULONG OutputBufferLength, _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat, _Out_ PULONG ResultantFormatLength);
private: PPORTTOPOLOGY m_Port; PUNKNOWN m_UnknownAdapter;
};

class CMiniportWaveRTLamaLoopbackCapture : public CMiniportWaveRT, public CUnknown
{
public: DECLARE_STD_UNKNOWN(); DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTLamaLoopbackCapture); ~CMiniportWaveRTLamaLoopbackCapture();
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port);
    NTSTATUS NewStream(_Out_ PMINIPORTWAVERTSTREAM* Stream, _In_ PPORTWAVERTSTREAM PortStream, _In_ ULONG Pin, _In_ BOOLEAN Capture, _In_ PKSDATAFORMAT DataFormat);
private: 
    PPORTWAVERT m_Port; 
    PUNKNOWN m_UnknownAdapter;
    ULONG m_MiniportInstanceIndex; // Added instance index
};
