#pragma once

// Include core audio driver headers
#include <ntddk.h>     // Core NT definitions
#include <wdm.h>       // Windows Driver Model
#include <ksmedia.h>    // For audio specific definitions
#include <portcls.h>   // For audio port class definitions

// Include project-specific headers
#include "sysvad.h"     // Include common definitions and kspindefs.h
#include "Common\common.h" // Include common structures and enums
#include "lamaloopbackcommon.h" // Include Lama Loopback specific definitions

// Define KS constants used in this file that might be missing
#ifndef KSPIN_FLAG_DISPATCH_LEVEL
#define KSPIN_FLAG_DISPATCH_LEVEL        0x00000001
#endif

#ifndef KSNODE_NONE
#define KSNODE_NONE ((ULONG)-1)
#endif

#ifndef KSFILTER_VERSION_DEVICE_SPECIFIC
#define KSFILTER_VERSION_DEVICE_SPECIFIC 0xFFFF0000
#endif

#ifndef MODE_RAW
#define MODE_RAW  0x00000001
#endif

#ifndef MODE_DEFAULT
#define MODE_DEFAULT 0x00000000
#endif

// Pin types
#ifndef KSPIN_TOPO_BRIDGE_IN
#define KSPIN_TOPO_BRIDGE_IN            0
#endif

#ifndef KSPIN_TOPO_LOOPBACK_OUT
#define KSPIN_TOPO_LOOPBACK_OUT         1
#endif

#ifndef KSPIN_WAVE_HOST_IN
#define KSPIN_WAVE_HOST_IN              0
#endif

#ifndef KSPIN_WAVE_BRIDGE_OUT
#define KSPIN_WAVE_BRIDGE_OUT           1
#endif

#ifndef KSPIN_TOPO_LOOPBACK_IN
#define KSPIN_TOPO_LOOPBACK_IN          0
#endif

#ifndef KSPIN_TOPO_BRIDGE_OUT
#define KSPIN_TOPO_BRIDGE_OUT           1
#endif

#ifndef KSPIN_WAVE_CAPTURE_HOST_OUT
#define KSPIN_WAVE_CAPTURE_HOST_OUT     0
#endif

#ifndef KSPIN_WAVE_BRIDGE_IN
#define KSPIN_WAVE_BRIDGE_IN            1
#endif

// Use the project's local headers
#include "lamaloopbackcommon.h" // For LamaLoopbackFilterAutomationTable extern declaration, PinDataRangesPcm, GUIDs etc.
#include "minipairs.h" 
#include "EndpointsCommon\mintopo.h" 
#include "EndpointsCommon\minwavert.h"

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

// Define a placeholder node to avoid empty array issues
static KSNODE_DESCRIPTOR LamaLoopbackRenderTopoNode = { 0 };
static KSNODE_DESCRIPTOR* LamaLoopbackRenderTopoNodes = &LamaLoopbackRenderTopoNode;
static KSTOPOLOGY_CONNECTION LamaLoopbackRenderTopoConnections[] =
{
    { KSPIN_TOPO_BRIDGE_IN, KSNODE_NONE, KSPIN_TOPO_LOOPBACK_OUT, KSNODE_NONE }
};

// Define the automation table properties for filter
static KSPROPERTY_ITEM LamaLoopbackRenderProperties[] =
{
    {
        KSPROPERTY_GENERAL_COMPONENTID,
        0,
        sizeof(KSCOMPONENTID),
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0
    }
};

// Define the property set table
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackRenderFilter, LamaLoopbackRenderProperties);

// Properly structured PCFILTER_DESCRIPTOR to avoid initialization errors
static
PCFILTER_DESCRIPTOR LamaLoopbackRenderTopologyFilterDescriptor =
{
    0,                                              // Version
    &AutomationLamaLoopbackRenderFilter,            // AutomationTable (use our properly defined table)
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    2,                                              // PinCount
    (PCPIN_DESCRIPTOR*)LamaLoopbackRenderTopoPins,  // Pins (properly cast)
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    1,                                              // NodeCount
    (PCNODE_DESCRIPTOR*)LamaLoopbackRenderTopoNodes, // Nodes (properly cast)
    1,                                              // ConnectionCount
    LamaLoopbackRenderTopoConnections,              // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories
};

//=============================================================================
// Render Wave Descriptors
//=============================================================================
// Define a simpler KSPIN_DISPATCH structure with fewer initializers
static const KSPIN_DISPATCH LamaRenderPinDispatch =
{
    NULL,                   // Create
    NULL,                   // Close
    NULL,                   // Process
    NULL,                   // Reset
    NULL,                   // SetState
    NULL,                   // GetState
    NULL,                   // SetFormat
    NULL,                   // SetDeviceState
    NULL,                   // Connect
    NULL                    // Disconnect
    // Removed Write and Read as they may not be part of this structure
};

// Corrected KSPIN_DESCRIPTOR_EX structures with the right number of initializers
static KSPIN_DESCRIPTOR_EX LamaLoopbackRenderWavePins[] =
{
    // KSPIN_WAVE_HOST_IN (ID 0)
    {
        &LamaRenderPinDispatch,                                    // Dispatch
        NULL,                                                      // AutomationTable
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, 
          KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_SINK, 
          &KSCATEGORY_AUDIO, &PINNAME_LamaLoopbackWaveIn, 0 },     // PinDescriptor
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING  // Flags
        // Removed the extra initializer that was causing the error
    },
    // KSPIN_WAVE_BRIDGE_OUT (ID 1)
    {
        NULL,                                                     // Dispatch
        NULL,                                                     // AutomationTable
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, 
          KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_NONE, 
          &LAMA_LOOPBACK_BRIDGE_PIN_IN, NULL, 0 },                // PinDescriptor
        KSPIN_FLAG_DISPATCH_LEVEL                                // Flags
        // Removed extra initializer
    }
};

// Define a placeholder node to avoid empty array issues
static KSNODE_DESCRIPTOR LamaLoopbackRenderWaveNode = { 0 };
static KSNODE_DESCRIPTOR* LamaLoopbackRenderWaveNodes = &LamaLoopbackRenderWaveNode;
static KSTOPOLOGY_CONNECTION LamaLoopbackRenderWaveConnections[] =
{
    { KSPIN_WAVE_HOST_IN, KSNODE_NONE, KSPIN_WAVE_BRIDGE_OUT, KSNODE_NONE }
};

// Define the automation table properties for wave filter
static KSPROPERTY_ITEM LamaLoopbackRenderWaveProperties[] =
{
    {
        KSPROPERTY_GENERAL_COMPONENTID,
        0,
        sizeof(KSCOMPONENTID),
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0
    }
};

// Define the property set table for wave filter
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackRenderWaveFilter, LamaLoopbackRenderWaveProperties);

// Properly structured wave filter descriptor
static
PCFILTER_DESCRIPTOR LamaLoopbackRenderWaveFilterDescriptor =
{
    0,                                              // Version
    &AutomationLamaLoopbackRenderWaveFilter,        // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    2,                                              // PinCount
    (PCPIN_DESCRIPTOR*)LamaLoopbackRenderWavePins,  // Pins (properly cast)
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    1,                                              // NodeCount
    (PCNODE_DESCRIPTOR*)LamaLoopbackRenderWaveNodes, // Nodes (properly cast)
    1,                                              // ConnectionCount
    LamaLoopbackRenderWaveConnections,              // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories
};

// Define wave formats supported by the render pin
static KSDATAFORMAT_WAVEFORMATEXTENSIBLE LamaLoopbackRenderFormats[] = 
{
    { 
        // Format for 48kHz 16-bit stereo PCM
        {   
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,      // Stereo
                48000,  // 48kHz
                192000, // Bytes per second
                4,      // Block alignment
                16,     // Bits per sample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,         // Valid bits per sample
            KSAUDIO_SPEAKER_STEREO, // Channel mask
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

// Define the signal processing modes supported
static MODE_AND_DEFAULT_FORMAT LamaLoopbackRenderModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
        (KSDATAFORMAT*)&LamaLoopbackRenderFormats[0] // Cast to KSDATAFORMAT* to fix conversion error
    }
};

// Define Lama Loopback render pin device formats using proper structure
// Define LamaLoopbackRenderPinDeviceFormatsAndModes using PIN_DEVICE_FORMATS_AND_MODES
PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackRenderPinDeviceFormatsAndModes[] =
{
    {
        SystemRenderPin,                // PinType
        LamaLoopbackRenderFormats,      // WaveFormats
        SIZEOF_ARRAY(LamaLoopbackRenderFormats), // WaveFormatsCount
        NULL,                           // ModeAndDefaultFormat
        0                               // ModeAndDefaultFormatCount
    }
};

//=============================================================================
// Render Miniport Class Definitions
//=============================================================================
// Fix multiple inheritance by inheriting only from CMiniportTopology
// (which already inherits from CUnknown)
class CMiniportTopologyLamaLoopbackRender : public CMiniportTopology
{ 
public: 
    DECLARE_STD_UNKNOWN();
    
    CMiniportTopologyLamaLoopbackRender(
        _In_ PUNKNOWN UnknownOuter,
        _In_ PCFILTER_DESCRIPTOR *FilterDesc,
        _In_ USHORT DeviceMaxChannels,
        _In_ eDeviceType DeviceType,
        _In_opt_ PVOID DeviceContext
    )
        : CMiniportTopology(UnknownOuter, FilterDesc, DeviceMaxChannels, DeviceType, DeviceContext)
    {
        m_Port = NULL;
        m_UnknownAdapter = NULL;
    }
    
    ~CMiniportTopologyLamaLoopbackRender();
    
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY PortTopology);
    
    NTSTATUS DataRangeIntersection(
        _In_ ULONG PinId, 
        _In_ PKSDATARANGE DataRange, 
        _In_ PKSDATARANGE MatchingDataRange, 
        _In_ ULONG OutputBufferLength, 
        _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat, 
        _Out_ PULONG ResultantFormatLength);
        
private: 
    PPORTTOPOLOGY m_Port; 
    PUNKNOWN m_UnknownAdapter;
};

// Fix multiple inheritance by inheriting only from CMiniportWaveRT
// (which already inherits from CUnknown)
class CMiniportWaveRTLamaLoopbackRender : public CMiniportWaveRT
{ 
public: 
    DECLARE_STD_UNKNOWN();
    
    CMiniportWaveRTLamaLoopbackRender(
        _In_            PUNKNOWN                UnknownAdapter,
        _In_            PENDPOINT_MINIPAIR      MiniportPair,
        _In_opt_        PVOID                   DeviceContext
    )
        : CMiniportWaveRT(UnknownAdapter, MiniportPair, DeviceContext)
    {
    }
    
    ~CMiniportWaveRTLamaLoopbackRender();
    
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, 
                  _In_ PRESOURCELIST ResourceList, 
                  _In_ PPORTWAVERT Port);
                  
    NTSTATUS NewStream(_Out_ PMINIPORTWAVERTSTREAM * Stream, 
                      _In_ PPORTWAVERTSTREAM PortStream, 
                      _In_ ULONG Pin, 
                      _In_ BOOLEAN Capture, 
                      _In_ PKSDATAFORMAT DataFormat);
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

// Define a placeholder node to avoid empty array issues
static KSNODE_DESCRIPTOR LamaLoopbackCaptureTopoNode = { 0 };
static KSNODE_DESCRIPTOR* LamaLoopbackCaptureTopoNodes = &LamaLoopbackCaptureTopoNode;
static KSTOPOLOGY_CONNECTION LamaLoopbackCaptureTopoConnections[] =
{ { KSPIN_TOPO_LOOPBACK_IN, KSNODE_NONE, KSPIN_TOPO_BRIDGE_OUT, KSNODE_NONE } };

// Define the automation table properties for capture topology filter
static KSPROPERTY_ITEM LamaLoopbackCaptureTopoProperties[] =
{
    {
        KSPROPERTY_GENERAL_COMPONENTID,
        0,
        sizeof(KSCOMPONENTID),
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0
    }
};

// Define the property set table for capture topology filter
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackCaptureTopoFilter, LamaLoopbackCaptureTopoProperties);

static
PCFILTER_DESCRIPTOR LamaLoopbackCaptureTopologyFilterDescriptor =
{
    0,                                                  // Version
    &AutomationLamaLoopbackCaptureTopoFilter,           // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                           // PinSize
    2,                                                  // PinCount
    (PCPIN_DESCRIPTOR*)LamaLoopbackCaptureTopoPins,     // Pins (properly cast)
    sizeof(PCNODE_DESCRIPTOR),                          // NodeSize
    1,                                                  // NodeCount
    (PCNODE_DESCRIPTOR*)LamaLoopbackCaptureTopoNodes,   // Nodes (properly cast)
    1,                                                  // ConnectionCount
    LamaLoopbackCaptureTopoConnections,                 // Connections
    0,                                                  // CategoryCount
    NULL                                                // Categories
};

//=============================================================================
// Capture Wave Descriptors
//=============================================================================
// Define a simpler KSPIN_DISPATCH structure with fewer initializers
static const KSPIN_DISPATCH LamaCapturePinDispatch =
{
    NULL,                   // Create
    NULL,                   // Close
    NULL,                   // Process
    NULL,                   // Reset
    NULL,                   // SetState
    NULL,                   // GetState
    NULL,                   // SetFormat
    NULL,                   // SetDeviceState
    NULL,                   // Connect
    NULL                    // Disconnect
    // Removed Write and Read as they may not be part of this structure
};

// Corrected KSPIN_DESCRIPTOR_EX structures with the right number of initializers
static KSPIN_DESCRIPTOR_EX LamaLoopbackCaptureWavePins[] =
{
    // KSPIN_WAVE_CAPTURE_HOST_OUT (ID 0)
    {
        &LamaCapturePinDispatch,                                  // Dispatch
        NULL,                                                     // AutomationTable
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, 
          KSPIN_DATAFLOW_OUT, KSPIN_COMMUNICATION_SOURCE, 
          &KSCATEGORY_AUDIO, &PINNAME_LamaLoopbackWaveOut, 0 },   // PinDescriptor
        KSPIN_FLAG_DISPATCH_LEVEL | KSPIN_FLAG_DO_NOT_INITIATE_PROCESSING  // Flags
        // Removed the extra initializer
    },
    // KSPIN_WAVE_BRIDGE_IN (ID 1)
    {
        NULL,                                                     // Dispatch
        NULL,                                                     // AutomationTable
        { 0, NULL, 0, NULL, SIZEOF_ARRAY(PinDataRangesPcm), PinDataRangesPcm, 
          KSPIN_DATAFLOW_IN, KSPIN_COMMUNICATION_NONE, 
          &KSCATEGORY_AUDIO, NULL, 0 },                          // PinDescriptor
        KSPIN_FLAG_DISPATCH_LEVEL                                // Flags
        // Removed the extra initializer
    }
};

// Define a placeholder node to avoid empty array issues
static KSNODE_DESCRIPTOR LamaLoopbackCaptureWaveNode = { 0 };
static KSNODE_DESCRIPTOR* LamaLoopbackCaptureWaveNodes = &LamaLoopbackCaptureWaveNode;
static KSTOPOLOGY_CONNECTION LamaLoopbackCaptureWaveConnections[] =
{ { KSPIN_WAVE_BRIDGE_IN, KSNODE_NONE, KSPIN_WAVE_CAPTURE_HOST_OUT, KSNODE_NONE } };

// Define the automation table properties for capture wave filter
static KSPROPERTY_ITEM LamaLoopbackCaptureWaveProperties[] =
{
    {
        KSPROPERTY_GENERAL_COMPONENTID,
        0,
        sizeof(KSCOMPONENTID),
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0
    }
};

// Define the property set table for capture wave filter
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationLamaLoopbackCaptureWaveFilter, LamaLoopbackCaptureWaveProperties);

// Properly structured filter descriptor for the capture wave filter
static
PCFILTER_DESCRIPTOR LamaLoopbackCaptureWaveFilterDescriptor =
{
    0,                                                  // Version
    &AutomationLamaLoopbackCaptureWaveFilter,           // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                           // PinSize
    2,                                                  // PinCount
    (PCPIN_DESCRIPTOR*)LamaLoopbackCaptureWavePins,     // Pins (properly cast)
    sizeof(PCNODE_DESCRIPTOR),                          // NodeSize
    1,                                                  // NodeCount
    (PCNODE_DESCRIPTOR*)LamaLoopbackCaptureWaveNodes,   // Nodes (properly cast)
    1,                                                  // ConnectionCount
    LamaLoopbackCaptureWaveConnections,                 // Connections
    0,                                                  // CategoryCount
    NULL                                                // Categories
};

// Define wave formats supported by the capture pin
static KSDATAFORMAT_WAVEFORMATEXTENSIBLE LamaLoopbackCaptureFormats[] = 
{
    { 
        // Format for 48kHz 16-bit stereo PCM
        {   
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,      // Stereo
                48000,  // 48kHz
                192000, // Bytes per second
                4,      // Block alignment
                16,     // Bits per sample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,         // Valid bits per sample
            KSAUDIO_SPEAKER_STEREO, // Channel mask
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

// Define the signal processing modes supported
static MODE_AND_DEFAULT_FORMAT LamaLoopbackCaptureModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
        (KSDATAFORMAT*)&LamaLoopbackCaptureFormats[0] // Cast to KSDATAFORMAT* to fix conversion error
    }
};

// Define Lama Loopback capture pin device formats using proper structure
// Use PIN_DEVICE_FORMATS_AND_MODES instead of SYSVAD_PIN_DEVICE_FORMATS_AND_MODES
PIN_DEVICE_FORMATS_AND_MODES LamaLoopbackCapturePinDeviceFormatsAndModes[] =
{
    {
        SystemCapturePin,              // PinType 
        LamaLoopbackCaptureFormats,     // WaveFormats
        SIZEOF_ARRAY(LamaLoopbackCaptureFormats), // WaveFormatsCount
        LamaLoopbackCaptureModes,       // ModeAndDefaultFormat
        SIZEOF_ARRAY(LamaLoopbackCaptureModes) // ModeAndDefaultFormatCount
    }
};

//=============================================================================
// Capture Miniport Class Definitions
//=============================================================================
// Fix multiple inheritance by inheriting only from CMiniportTopology
// (which already inherits from CUnknown)
class CMiniportTopologyLamaLoopbackCapture : public CMiniportTopology
{
public: 
    DECLARE_STD_UNKNOWN(); 
    
    CMiniportTopologyLamaLoopbackCapture(
        _In_ PUNKNOWN UnknownOuter,
        _In_ PCFILTER_DESCRIPTOR *FilterDesc,
        _In_ USHORT DeviceMaxChannels,
        _In_ eDeviceType DeviceType,
        _In_opt_ PVOID DeviceContext
    )
        : CMiniportTopology(UnknownOuter, FilterDesc, DeviceMaxChannels, DeviceType, DeviceContext)
    {
        m_Port = NULL;
        m_UnknownAdapter = NULL;
    }
    
    ~CMiniportTopologyLamaLoopbackCapture();
    
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTTOPOLOGY PortTopology);
    
    NTSTATUS DataRangeIntersection(
        _In_ ULONG PinId, 
        _In_ PKSDATARANGE DataRange, 
        _In_ PKSDATARANGE MatchingDataRange, 
        _In_ ULONG OutputBufferLength, 
        _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength) PVOID ResultantFormat, 
        _Out_ PULONG ResultantFormatLength);
        
private: 
    PPORTTOPOLOGY m_Port; 
    PUNKNOWN m_UnknownAdapter;
};

// Fix multiple inheritance by inheriting only from CMiniportWaveRT
// (which already inherits from CUnknown)
class CMiniportWaveRTLamaLoopbackCapture : public CMiniportWaveRT
{
public: 
    DECLARE_STD_UNKNOWN(); 
    
    CMiniportWaveRTLamaLoopbackCapture(
        _In_            PUNKNOWN                UnknownAdapter,
        _In_            PENDPOINT_MINIPAIR      MiniportPair,
        _In_opt_        PVOID                   DeviceContext
    )
        : CMiniportWaveRT(UnknownAdapter, MiniportPair, DeviceContext)
    {
    }
    
    ~CMiniportWaveRTLamaLoopbackCapture();
    
    NTSTATUS Init(_In_ PUNKNOWN UnknownAdapter, _In_ PRESOURCELIST ResourceList, _In_ PPORTWAVERT Port);
    
    NTSTATUS NewStream(
        _Out_ PMINIPORTWAVERTSTREAM* Stream, 
        _In_ PPORTWAVERTSTREAM PortStream, 
        _In_ ULONG Pin, 
        _In_ BOOLEAN Capture, 
        _In_ PKSDATAFORMAT DataFormat);
private: 
    PPORTWAVERT m_Port; 
    PUNKNOWN m_UnknownAdapter;
    ULONG m_MiniportInstanceIndex; // Added instance index
};
